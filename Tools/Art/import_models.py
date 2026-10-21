"""Imports the CC0 models fetched by fetch_models.py as Nanite static meshes, headless.

Each model lands in /Game/Art/Models/<Role>/. A model that imports as a single mesh is renamed SM_<Role>,
which is the name the game loads; a multi-part model (the modular poles, the fence) keeps its parts' own
names, listed in the log. Nanite is switched on for everything, and the photogrammetry trees -- the pine
alone is ~950 MB of glTF because every needle is modelled -- are trimmed to a triangle budget so the
committed assets stay a sensible size. Nanite still draws them at full apparent detail at any distance.

Run from the repo root:

  UnrealEditor-Cmd.exe DustToGlory.uproject -run=pythonscript -script="Tools/Art/import_models.py"
      -unattended -nosplash -stdout

Roles already imported are skipped; pass --reimport to redo them.
"""
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(__file__))
from fetch_models import MODELS  # noqa: E402  (the role -> asset table is shared with the fetcher)

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
SOURCE = os.path.join(ROOT, "ArtSource", "PolyHavenModels")
DEST = "/Game/Art/Models"

# Most triangles a single mesh keeps. Nanite makes the cost of a dense mesh mostly disk and memory,
# and a forest instances the same tree thousands of times, so this is about repository size.
TRIANGLE_BUDGET = 600_000
# Foliage: Nanite's area preservation stops thin leaves and needles from eroding away as it simplifies.
VEGETATION = {"Pine", "Fir", "Palm1", "Palm2", "Palm3", "Jacaranda", "SmallTree", "Bermuda", "GrassA",
              "GrassB", "Shrub1", "Shrub2", "Shrub3", "Shrub4", "Fern"}
# The needle trees: millions of needle triangles that the legacy area-preserving mode collapsed into
# flat dark flags a hundred metres off (the "dead pines" of Milestones 2 and 3). Voxelize keeps a cloud a cloud.
CONIFERS = {"Pine", "Fir"}

assets = unreal.EditorAssetLibrary


REPORT = os.path.join(ROOT, "Saved", "import_models.log")


def log(message):
    # The commandlet only echoes warnings and errors to stdout, so the report also goes to a file.
    unreal.log("DTGModels: " + message)
    with open(REPORT, "a", encoding="utf-8") as stream:
        stream.write(message + "\n")


def static_meshes(folder):
    found = []
    for path in assets.list_assets(folder, recursive=True):
        asset = unreal.load_asset(path)
        if isinstance(asset, unreal.StaticMesh) and asset not in found:
            found.append(asset)
    return found


def finish_role(role, asset_id):
    """Nanite on every mesh, trimmed to budget; the main one renamed SM_<Role> for the game to load.

    Headless, so no StaticMeshEditorSubsystem (it needs the editor UI): the settings are set on the
    asset and saved, and the Nanite data is built from them when the mesh is next loaded.
    """
    folder = DEST + "/" + role
    meshes = static_meshes(folder)
    if not meshes:
        unreal.log_warning("DTGModels: %s: no static mesh under %s" % (role, folder))
        return
    for mesh in meshes:
        tris = mesh.get_num_triangles(0)
        settings = mesh.get_editor_property("nanite_settings")
        settings.set_editor_property("enabled", True)
        # Trimming wrecks foliage: a leaf card cut from a quad to a triangle keeps its texture coordinates
        # and shows a dark triangle instead of a leaf, which the first textured run showed on every
        # trimmed tree. Foliage stays whole and Nanite's own LOD does the work; the models are not
        # committed (see .gitignore), so their size only costs local disk.
        trim = tris > TRIANGLE_BUDGET and role not in VEGETATION
        settings.set_editor_property("keep_percent_triangles", max(0.01, TRIANGLE_BUDGET / float(tris)) if trim else 1.0)
        if role in VEGETATION and hasattr(unreal, "NaniteShapePreservation"):
            # UE 5.8's foliage modes: area preservation (the legacy one) for leaves, voxelize for needles.
            preservation = unreal.NaniteShapePreservation.PRESERVE_AREA
            if role in CONIFERS and hasattr(unreal.NaniteShapePreservation, "VOXELIZE"):
                preservation = unreal.NaniteShapePreservation.VOXELIZE
            settings.set_editor_property("shape_preservation", preservation)
        mesh.set_editor_property("nanite_settings", settings)
        log("%s: %s, %d triangles%s" % (role, mesh.get_name(), tris,
            " (keeping %.0f%%)" % (100.0 * TRIANGLE_BUDGET / tris) if trim else ""))
    # A model can import as several meshes (the hydrant brings aged and cap variants): the main one is
    # the one named for the asset, or failing that the densest.
    named = [mesh for mesh in meshes if mesh.get_name() == asset_id]
    main = named[0] if named else max(meshes, key=lambda mesh: mesh.get_num_triangles(0))
    if main.get_name() != "SM_" + role and not assets.does_asset_exist(folder + "/SM_" + role):
        assets.rename_asset(main.get_path_name().split(".")[0], folder + "/SM_" + role)
    log("%s: main mesh -> SM_%s (%d meshes in all)" % (role, role, len(meshes)))


def import_role(role, asset_id, reimport):
    folder = DEST + "/" + role
    if assets.does_directory_exist(folder) and not reimport:
        return
    source_dir = os.path.join(SOURCE, asset_id)
    gltf = [name for name in os.listdir(source_dir) if name.endswith(".gltf")] if os.path.isdir(source_dir) else []
    if not gltf:
        unreal.log_warning("DTGModels: %s: no glTF under %s; run fetch_models.py" % (role, source_dir))
        return
    if assets.does_directory_exist(folder):
        assets.delete_directory(folder)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", os.path.join(source_dir, gltf[0]))
    task.set_editor_property("destination_path", folder)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", False)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    log("%s: imported from %s" % (role, gltf[0]))


MASTERS = "/Game/Art/Materials"
GLTF_BASE = "/InterchangeAssets/gltf/M_Default"
mel = unreal.MaterialEditingLibrary


def build_model_master(name, masked):
    """A master for the imported glTF materials, using the importer's own parameter names.

    The importer's instances all hang off engine content (/InterchangeAssets/gltf/M_Default) that is
    not flagged for instanced static meshes, so a game drawing trees as instances falls back to the
    default material -- the first run showed them black. And it makes leaves translucent, which casts
    no shadow and cannot be Nanite. These two masters are the project's own: opaque, and masked
    two-sided for foliage. Matching the parameter names means re-parenting keeps every texture.
    """
    path = MASTERS + "/" + name
    if assets.does_asset_exist(path):
        assets.delete_asset(path)
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, MASTERS, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property("two_sided", True)
    material.set_editor_property("used_with_instanced_static_meshes", True)
    # Nanite only runs on SM6; the first SM6 run showed every model on the default material without this.
    material.set_editor_property("used_with_nanite", True)
    if masked:
        material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
        # 0.3, not 0.4: the pine's alpha trims a tenth of its needle edges at 0.4, at every mip.
        material.set_editor_property("opacity_mask_clip_value", 0.3)
        # Foliage shading: light comes through a leaf, so a crown with the sun behind it glows rather
        # than going black, which is most of what made the pines read as dead.
        material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_TWO_SIDED_FOLIAGE)
    base = unreal.load_asset(GLTF_BASE)

    def texture(param, sampler, y):
        node = mel.create_material_expression(material, unreal.MaterialExpressionTextureSampleParameter2D, -700, y)
        node.set_editor_property("parameter_name", param)
        node.set_editor_property("sampler_type", sampler)
        default = mel.get_material_default_texture_parameter_value(base, param) if base else None
        if default:
            node.set_editor_property("texture", default)
        return node

    def scalar(param, value, y):
        node = mel.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -700, y)
        node.set_editor_property("parameter_name", param)
        node.set_editor_property("default_value", value)
        return node

    def multiply(a, a_out, b, b_out, y):
        node = mel.create_material_expression(material, unreal.MaterialExpressionMultiply, -350, y)
        mel.connect_material_expressions(a, a_out, node, "A")
        mel.connect_material_expressions(b, b_out, node, "B")
        return node

    colour = texture("BaseColorTexture", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, -400)
    factor = mel.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -700, -250)
    factor.set_editor_property("parameter_name", "BaseColorFactor")
    factor.set_editor_property("default_value", unreal.LinearColor(1.0, 1.0, 1.0, 1.0))
    mel.connect_material_property(multiply(colour, "RGB", factor, "RGB", -400), "", unreal.MaterialProperty.MP_BASE_COLOR)
    if masked:
        # The leaf's own colour, dimmed, as what the light through it carries.
        mel.connect_material_property(multiply(colour, "RGB", scalar("SubsurfaceFactor", 0.6, -300), "", -300), "",
                                      unreal.MaterialProperty.MP_SUBSURFACE_COLOR)
        # Poly Haven ships leaf colour as JPG (no alpha) and the cut-out as a separate map, so the
        # mask is OpacityTexture (white by default) times the colour's own alpha: either source works.
        opacity = texture("OpacityTexture", unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR, -550)
        if base:
            opacity.set_editor_property("texture", mel.get_material_default_texture_parameter_value(base, "OcclusionTexture"))
        mel.connect_material_property(multiply(opacity, "R", colour, "A", -550), "", unreal.MaterialProperty.MP_OPACITY_MASK)

    # glTF packs roughness in green and metalness in blue.
    packed = texture("MetallicRoughnessTexture", unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR, -100)
    mel.connect_material_property(multiply(packed, "G", scalar("RoughnessFactor", 1.0, 50), "", -100),
                                  "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(multiply(packed, "B", scalar("MetallicFactor", 0.0, 150), "", 0),
                                  "", unreal.MaterialProperty.MP_METALLIC)
    normal = texture("NormalTexture", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, 250)
    mel.connect_material_property(normal, "RGB", unreal.MaterialProperty.MP_NORMAL)
    occlusion = texture("OcclusionTexture", unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR, 450)
    mel.connect_material_property(occlusion, "R", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    mel.recompile_material(material)
    return material


CUTOUT_WORDS = ("leaf", "leaves", "foliage", "frond", "needle", "grass", "fern", "twig")


def wants_cutout(instance, parent_name):
    """Leaves, fronds and grass need the masked master; the importer does not always say so.

    Its Blend/Mask parents mark some, but the tree leaves came in opaque and drew as solid dark
    triangles. A colour texture the importer merged with an alpha map, or a leafy name, also counts.
    """
    if "Blend" in parent_name or "Mask" in parent_name:
        return True
    name = instance.get_name().lower()
    if any(word in name for word in CUTOUT_WORDS):
        return True
    colour = mel.get_material_instance_texture_parameter_value(instance, "BaseColorTexture")
    return bool(colour) and "alpha" in colour.get_name().lower()


def alpha_source(instance, asset_id):
    """The Poly Haven alpha map for this material, if there is one: <asset>_<part>_alpha or <asset>_alpha."""
    textures = os.path.join(SOURCE, asset_id, "textures")
    if not os.path.isdir(textures):
        return None
    part = instance.get_name()[len(asset_id):].strip("_") if instance.get_name().startswith(asset_id) else ""
    stem = "%s_%s_alpha" % (asset_id, part) if part else "%s_alpha" % asset_id
    for name in os.listdir(textures):
        if name.startswith(stem + "_") and name.lower().endswith(".jpg"):
            return os.path.join(textures, name)
    return None


def alpha_texture(source, folder, instance):
    name = "T_%s_Alpha" % instance.get_name()
    path = folder + "/Alpha/" + name
    if not assets.does_asset_exist(path):
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", source)
        task.set_editor_property("destination_path", folder + "/Alpha")
        task.set_editor_property("destination_name", name)
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("save", False)
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture = unreal.load_asset(path)
    if texture:
        texture.set_editor_property("srgb", False)
    return texture


def reparent_model_materials(opaque, masked):
    moved = 0
    cutouts = []
    for path in assets.list_assets(DEST, recursive=True):
        instance = unreal.load_asset(path)
        if not isinstance(instance, unreal.MaterialInstanceConstant):
            continue
        parent = instance.get_editor_property("parent")
        parent_name = parent.get_name() if parent else ""
        role = path[len(DEST) + 1:].split("/")[0]
        asset_id = MODELS.get(role, "")
        source = alpha_source(instance, asset_id) if asset_id else None
        target = masked if (source or wants_cutout(instance, parent_name)) else opaque
        if target == masked and source:
            alpha = alpha_texture(source, DEST + "/" + role, instance)
            if alpha:
                mel.set_material_instance_texture_parameter_value(instance, "OpacityTexture", alpha)
        if parent == target and not source:
            continue
        # Re-parenting from one of our masters to the other loses nothing: the names match.
        mel.set_material_instance_parent(instance, target)
        moved += 1
        if target == masked:
            cutouts.append(instance.get_name())
    log("re-parented %d model materials; masked: %s" % (moved, ", ".join(cutouts)))


def main():
    reimport = "--reimport" in unreal.SystemLibrary.get_command_line()
    for role, asset_id in MODELS.items():
        try:
            import_role(role, asset_id, reimport)
            finish_role(role, asset_id)  # idempotent, so settings changes here always reach the assets
        except Exception as error:  # one bad model should not stop the rest
            unreal.log_error("DTGModels: %s failed: %s" % (role, error))
    opaque = build_model_master("M_DTG_Model", masked=False)
    masked = build_model_master("M_DTG_ModelMasked", masked=True)
    reparent_model_materials(opaque, masked)
    assets.save_directory(MASTERS, only_if_is_dirty=False, recursive=True)
    assets.save_directory(DEST, only_if_is_dirty=False, recursive=True)
    log("saved " + DEST)


main()
