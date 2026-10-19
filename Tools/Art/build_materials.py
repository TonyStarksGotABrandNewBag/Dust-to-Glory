"""Builds the world's materials inside the Unreal editor, headless.

Imports the photo-scanned textures fetched by fetch_textures.py and builds:

  M_DTG_Surface   triplanar PBR for generated geometry: colour, normal and ORM projected from world
                  space, so procedural meshes need no UVs. Vertex colour can tint it (TintStrength).
  M_DTG_Paint     clear-coat car paint, colour from the vertex colour.
  M_DTG_Plain     vertex colour with roughness/metallic/colour-scale parameters: glass, chrome,
                  rubber, skin, cloth, leaves, and the fallback for anything not yet classified.

and one material instance per surface kind under /Game/Art/Materials. Run from the repo root:

  UnrealEditor-Cmd.exe DustToGlory.uproject -run=pythonscript -script="Tools/Art/build_materials.py"
      -unattended -nosplash -stdout

Re-running replaces everything it made; nothing else under /Game is touched.
"""
import os

import unreal

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
SOURCE = os.path.join(ROOT, "ArtSource", "PolyHaven")
TEXTURES = "/Game/Art/Textures"
MATERIALS = "/Game/Art/Materials"

mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()

# Kind -> (tile size in cm, how much the vertex colour tints it, roughness scale).
# Tint 0 keeps the photo's own colour (roads, ground, bark); facades take the building's colour.
SURFACES = {
    "Asphalt": (400.0, 0.0, 1.0),
    "Sidewalk": (200.0, 0.0, 1.0),
    "Concrete": (300.0, 0.35, 1.0),
    "Brick": (180.0, 0.25, 1.0),
    "Stucco": (250.0, 0.85, 1.0),
    "WoodSiding": (200.0, 0.9, 1.0),
    "ShingleRoof": (300.0, 0.5, 1.0),
    "MetalRoof": (250.0, 0.6, 0.8),
    "Grass": (300.0, 0.0, 1.0),
    "Sand": (300.0, 0.0, 1.0),
    "Dirt": (300.0, 0.0, 1.0),
    "ForestFloor": (300.0, 0.0, 1.0),
    "PineBark": (120.0, 0.0, 1.0),
    "PalmBark": (120.0, 0.0, 1.0),
    "Bark": (120.0, 0.0, 1.0),
}

# Kind -> (roughness, metallic, colour scale) on M_DTG_Plain.
PLAIN = {
    "Matte": (0.75, 0.0, 1.0),
    "Glass": (0.04, 0.9, 0.25),
    "Chrome": (0.12, 1.0, 1.0),
    "Rubber": (0.9, 0.0, 0.6),
    "Skin": (0.55, 0.0, 1.0),
    "Cloth": (0.85, 0.0, 1.0),
    "Leaves": (0.65, 0.0, 1.0),
    "Water": (0.03, 0.0, 1.0),
}

# The terrain's vertex colours are class colours set in DTGWorldBuilder::BuildChunk. The terrain material
# turns each back into a weight -- grass, sand (beach, sea bed), clay (lake bed, upland rock) -- so the
# grid blends smoothly between them and still reads correctly with the plain vertex-colour fallback.
TERRAIN_WEIGHTS = """
float3 Grass = float3(0.17, 0.30, 0.12);
float3 Beach = float3(0.62, 0.56, 0.42); float3 SeaBed = float3(0.50, 0.45, 0.34);
float3 LakeBed = float3(0.22, 0.20, 0.16); float3 Rock = float3(0.30, 0.29, 0.27);
float WG = exp(-80.0 * dot(C - Grass, C - Grass));
float WS = exp(-80.0 * min(dot(C - Beach, C - Beach), dot(C - SeaBed, C - SeaBed)));
float WD = exp(-80.0 * min(dot(C - LakeBed, C - LakeBed), dot(C - Rock, C - Rock)));
float3 Wt = float3(WG, WS, WD) + 1e-4; Wt /= dot(Wt, float3(1.0, 1.0, 1.0));
float2 Uv = P.xy;
"""

TERRAIN_COLOUR = TERRAIN_WEIGHTS + """
return Texture2DSample(GrassD, GrassDSampler, Uv).rgb * Wt.x
     + Texture2DSample(SandD, SandDSampler, Uv).rgb * Wt.y
     + Texture2DSample(DirtD, DirtDSampler, Uv).rgb * Wt.z;
"""

TERRAIN_ORM = TERRAIN_WEIGHTS + """
return Texture2DSample(GrassO, GrassOSampler, Uv).rgb * Wt.x
     + Texture2DSample(SandO, SandOSampler, Uv).rgb * Wt.y
     + Texture2DSample(DirtO, DirtOSampler, Uv).rgb * Wt.z;
"""

TERRAIN_NORMAL = TERRAIN_WEIGHTS + """
float2 T = (Texture2DSample(GrassN, GrassNSampler, Uv).rg * 2.0 - 1.0) * Wt.x
         + (Texture2DSample(SandN, SandNSampler, Uv).rg * 2.0 - 1.0) * Wt.y
         + (Texture2DSample(DirtN, DirtNSampler, Uv).rg * 2.0 - 1.0) * Wt.z;
float3 N = normalize(Nrm);
float3 TN = float3(T, sqrt(saturate(1.0 - dot(T, T))));
return normalize(float3(TN.xy + N.xy, abs(TN.z) * N.z));
"""

# Shared by the three triplanar nodes: weights and per-axis UVs (after Ben Golus's whiteout triplanar).
TRIPLANAR_SETUP = """
float3 N = normalize(Nrm);
float3 W = pow(abs(N), 4.0); W /= dot(W, float3(1.0, 1.0, 1.0));
float3 S = float3(N.x < 0 ? -1.0 : 1.0, N.y < 0 ? -1.0 : 1.0, N.z < 0 ? -1.0 : 1.0);
float2 UvX = P.zy; float2 UvY = P.xz; float2 UvZ = P.xy;
UvX.x *= S.x; UvY.x *= S.y; UvZ.x *= -S.z;
"""

COLOUR_CODE = TRIPLANAR_SETUP + """
float3 C = Texture2DSample(Tex, TexSampler, UvX).rgb * W.x
         + Texture2DSample(Tex, TexSampler, UvY).rgb * W.y
         + Texture2DSample(Tex, TexSampler, UvZ).rgb * W.z;
float Lum = dot(C, float3(0.3, 0.59, 0.11));
return lerp(C, saturate(Lum * 2.0 * Tint), TintStrength);
"""

ORM_CODE = TRIPLANAR_SETUP + """
return Texture2DSample(Tex, TexSampler, UvX).rgb * W.x
     + Texture2DSample(Tex, TexSampler, UvY).rgb * W.y
     + Texture2DSample(Tex, TexSampler, UvZ).rgb * W.z;
"""

NORMAL_CODE = TRIPLANAR_SETUP + """
float2 Nx = Texture2DSample(Tex, TexSampler, UvX).rg * 2.0 - 1.0;
float2 Ny = Texture2DSample(Tex, TexSampler, UvY).rg * 2.0 - 1.0;
float2 Nz = Texture2DSample(Tex, TexSampler, UvZ).rg * 2.0 - 1.0;
float3 TX = float3(Nx, sqrt(saturate(1.0 - dot(Nx, Nx))));
float3 TY = float3(Ny, sqrt(saturate(1.0 - dot(Ny, Ny))));
float3 TZ = float3(Nz, sqrt(saturate(1.0 - dot(Nz, Nz))));
TX.x *= S.x; TY.x *= S.y; TZ.x *= -S.z;
TX = float3(TX.xy + N.zy, abs(TX.z) * N.x);
TY = float3(TY.xy + N.xz, abs(TY.z) * N.y);
TZ = float3(TZ.xy + N.xy, abs(TZ.z) * N.z);
return normalize(TX.zyx * W.x + TY.xzy * W.y + TZ.xyz * W.z);
"""


def log(message):
    unreal.log("DTGArt: " + message)


def import_textures():
    tasks = []
    for name in sorted(os.listdir(SOURCE)):
        if not name.lower().endswith(".jpg"):
            continue
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", os.path.join(SOURCE, name))
        task.set_editor_property("destination_path", TEXTURES)
        task.set_editor_property("destination_name", os.path.splitext(name)[0])
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("save", False)
        tasks.append(task)
    tools.import_asset_tasks(tasks)
    for task in tasks:
        for path in task.get_editor_property("imported_object_paths"):
            texture = unreal.load_asset(path)
            name = texture.get_name()
            if name.endswith("_N"):
                texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
                texture.set_editor_property("srgb", False)
            elif name.endswith("_ORM"):
                texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
                texture.set_editor_property("srgb", False)
            texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD)
    log("imported %d textures" % len(tasks))


def fresh_material(name):
    path = MATERIALS + "/" + name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.delete_asset(path)
    return tools.create_asset(name, MATERIALS, unreal.Material, unreal.MaterialFactoryNew())


def expr(material, cls, x, y):
    return mel.create_material_expression(material, cls, x, y)


def scalar(material, name, value, x, y):
    node = expr(material, unreal.MaterialExpressionScalarParameter, x, y)
    node.set_editor_property("parameter_name", name)
    node.set_editor_property("default_value", value)
    return node


def custom(material, code, output_type, inputs, x, y, description):
    node = expr(material, unreal.MaterialExpressionCustom, x, y)
    node.set_editor_property("code", code)
    node.set_editor_property("output_type", output_type)
    node.set_editor_property("description", description)
    pins = []
    for name in inputs:
        pin = unreal.CustomInput()
        pin.set_editor_property("input_name", name)
        pins.append(pin)
    node.set_editor_property("inputs", pins)
    return node


def tile_position(material, tile_cm):
    """World position in tiles, wrapped every 64 tiles so the large-world coordinates stay precise."""
    world = expr(material, unreal.MaterialExpressionWorldPosition, -1600, 0)
    tile = scalar(material, "TileCm", tile_cm, -1600, 150)
    in_tiles = expr(material, unreal.MaterialExpressionDivide, -1400, 0)
    mel.connect_material_expressions(world, "", in_tiles, "A")
    mel.connect_material_expressions(tile, "", in_tiles, "B")
    per_wrap = expr(material, unreal.MaterialExpressionDivide, -1250, 0)
    per_wrap.set_editor_property("const_b", 64.0)
    mel.connect_material_expressions(in_tiles, "", per_wrap, "A")
    wrapped = expr(material, unreal.MaterialExpressionFrac, -1100, 0)
    mel.connect_material_expressions(per_wrap, "", wrapped, "")
    position = expr(material, unreal.MaterialExpressionMultiply, -950, 0)
    position.set_editor_property("const_b", 64.0)
    mel.connect_material_expressions(wrapped, "", position, "A")
    return position


def build_terrain_master():
    material = fresh_material("M_DTG_Terrain")
    material.set_editor_property("tangent_space_normal", False)
    position = tile_position(material, 450.0)
    normal_ws = expr(material, unreal.MaterialExpressionVertexNormalWS, -950, 200)
    vertex_colour = expr(material, unreal.MaterialExpressionVertexColor, -950, 350)

    def texture(name, y, sampler):
        node = expr(material, unreal.MaterialExpressionTextureObject, -950, y)
        node.set_editor_property("texture", unreal.load_asset(TEXTURES + "/" + name))
        node.set_editor_property("sampler_type", sampler)
        return node

    colour_s = unreal.MaterialSamplerType.SAMPLERTYPE_COLOR
    normal_s = unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL
    mask_s = unreal.MaterialSamplerType.SAMPLERTYPE_MASKS
    float3 = unreal.CustomMaterialOutputType.CMOT_FLOAT3
    outputs = {}
    for row, (suffix, code, sampler, label) in enumerate((("D", TERRAIN_COLOUR, colour_s, "Terrain colour"),
                                                           ("N", TERRAIN_NORMAL, normal_s, "Terrain normal"),
                                                           ("O", TERRAIN_ORM, mask_s, "Terrain ORM"))):
        file_suffix = {"D": "D", "N": "N", "O": "ORM"}[suffix]
        names = ["Grass" + suffix, "Sand" + suffix, "Dirt" + suffix]
        node = custom(material, code, float3, names + ["C", "P", "Nrm"], -600, -700 + row * 250, label)
        for col, kind in enumerate(("Grass", "Sand", "Dirt")):
            tex = texture("T_%s_%s" % (kind, file_suffix), -900 + row * 250 + col * 60, sampler)
            mel.connect_material_expressions(tex, "", node, names[col])
        mel.connect_material_expressions(vertex_colour, "", node, "C")
        mel.connect_material_expressions(position, "", node, "P")
        mel.connect_material_expressions(normal_ws, "", node, "Nrm")
        outputs[suffix] = node

    def channel(r, g, b, y):
        mask = expr(material, unreal.MaterialExpressionComponentMask, -300, y)
        for flag, value in (("r", r), ("g", g), ("b", b), ("a", False)):
            mask.set_editor_property(flag, value)
        mel.connect_material_expressions(outputs["O"], "", mask, "")
        return mask

    mel.connect_material_property(outputs["D"], "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(outputs["N"], "", unreal.MaterialProperty.MP_NORMAL)
    mel.connect_material_property(channel(True, False, False, -250), "", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    mel.connect_material_property(channel(False, True, False, -150), "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(material)
    return material


def build_surface_master():
    material = fresh_material("M_DTG_Surface")
    material.set_editor_property("tangent_space_normal", False)

    # P: world position in tiles, wrapped every 64 tiles so the large-world coordinates stay precise.
    world = expr(material, unreal.MaterialExpressionWorldPosition, -1600, 0)
    tile = scalar(material, "TileCm", 300.0, -1600, 150)
    in_tiles = expr(material, unreal.MaterialExpressionDivide, -1400, 0)
    mel.connect_material_expressions(world, "", in_tiles, "A")
    mel.connect_material_expressions(tile, "", in_tiles, "B")
    per_wrap = expr(material, unreal.MaterialExpressionDivide, -1250, 0)
    per_wrap.set_editor_property("const_b", 64.0)
    mel.connect_material_expressions(in_tiles, "", per_wrap, "A")
    wrapped = expr(material, unreal.MaterialExpressionFrac, -1100, 0)
    mel.connect_material_expressions(per_wrap, "", wrapped, "")
    position = expr(material, unreal.MaterialExpressionMultiply, -950, 0)
    position.set_editor_property("const_b", 64.0)
    mel.connect_material_expressions(wrapped, "", position, "A")

    normal_ws = expr(material, unreal.MaterialExpressionVertexNormalWS, -950, 200)
    vertex_colour = expr(material, unreal.MaterialExpressionVertexColor, -950, 350)
    tint = scalar(material, "TintStrength", 0.0, -950, 500)
    rough_scale = scalar(material, "RoughnessScale", 1.0, -300, 700)

    def texture_param(name, sampler, y):
        node = expr(material, unreal.MaterialExpressionTextureObjectParameter, -950, y)
        node.set_editor_property("parameter_name", name)
        node.set_editor_property("sampler_type", sampler)
        return node

    colour_tex = texture_param("Colour", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, -600)
    normal_tex = texture_param("Normal", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, -400)
    orm_tex = texture_param("ORM", unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, -200)
    colour_tex.set_editor_property("texture", unreal.load_asset(TEXTURES + "/T_Concrete_D"))
    normal_tex.set_editor_property("texture", unreal.load_asset(TEXTURES + "/T_Concrete_N"))
    orm_tex.set_editor_property("texture", unreal.load_asset(TEXTURES + "/T_Concrete_ORM"))

    float3 = unreal.CustomMaterialOutputType.CMOT_FLOAT3
    colour = custom(material, COLOUR_CODE, float3, ["Tex", "P", "Nrm", "Tint", "TintStrength"], -600, -600, "Triplanar colour")
    normal = custom(material, NORMAL_CODE, float3, ["Tex", "P", "Nrm"], -600, -400, "Triplanar world normal")
    orm = custom(material, ORM_CODE, float3, ["Tex", "P", "Nrm"], -600, -200, "Triplanar ORM")
    for node, tex in ((colour, colour_tex), (normal, normal_tex), (orm, orm_tex)):
        mel.connect_material_expressions(tex, "", node, "Tex")
        mel.connect_material_expressions(position, "", node, "P")
        mel.connect_material_expressions(normal_ws, "", node, "Nrm")
    mel.connect_material_expressions(vertex_colour, "", colour, "Tint")
    mel.connect_material_expressions(tint, "", colour, "TintStrength")

    def channel(r, g, b, y):
        mask = expr(material, unreal.MaterialExpressionComponentMask, -300, y)
        mask.set_editor_property("r", r)
        mask.set_editor_property("g", g)
        mask.set_editor_property("b", b)
        mask.set_editor_property("a", False)
        mel.connect_material_expressions(orm, "", mask, "")
        return mask

    ao = channel(True, False, False, -250)
    rough = channel(False, True, False, -150)
    metal = channel(False, False, True, -50)
    rough_scaled = expr(material, unreal.MaterialExpressionMultiply, -150, -150)
    mel.connect_material_expressions(rough, "", rough_scaled, "A")
    mel.connect_material_expressions(rough_scale, "", rough_scaled, "B")

    mel.connect_material_property(colour, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(normal, "", unreal.MaterialProperty.MP_NORMAL)
    mel.connect_material_property(ao, "", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    mel.connect_material_property(rough_scaled, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(metal, "", unreal.MaterialProperty.MP_METALLIC)
    mel.recompile_material(material)
    return material


def build_paint_master():
    # Default lit rather than clear coat: UE 5.8's Python API does not expose the clear-coat inputs
    # (MaterialProperty has no CustomData pins). Roughness and metallic per finish carry most of the
    # difference between gloss, metallic, matte, pearl and chrome; a hand-built clear coat can replace it.
    material = fresh_material("M_DTG_Paint")
    vertex_colour = expr(material, unreal.MaterialExpressionVertexColor, -600, 0)
    metallic = scalar(material, "Metallic", 0.1, -600, 200)
    roughness = scalar(material, "Roughness", 0.18, -600, 300)
    specular = scalar(material, "Specular", 0.6, -600, 400)
    mel.connect_material_property(vertex_colour, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(metallic, "", unreal.MaterialProperty.MP_METALLIC)
    mel.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(specular, "", unreal.MaterialProperty.MP_SPECULAR)
    mel.recompile_material(material)
    return material


def build_plain_master():
    material = fresh_material("M_DTG_Plain")
    vertex_colour = expr(material, unreal.MaterialExpressionVertexColor, -600, 0)
    colour_scale = scalar(material, "ColourScale", 1.0, -600, 150)
    scaled = expr(material, unreal.MaterialExpressionMultiply, -350, 0)
    mel.connect_material_expressions(vertex_colour, "", scaled, "A")
    mel.connect_material_expressions(colour_scale, "", scaled, "B")
    roughness = scalar(material, "Roughness", 0.75, -600, 300)
    metallic = scalar(material, "Metallic", 0.0, -600, 400)
    mel.connect_material_property(scaled, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(metallic, "", unreal.MaterialProperty.MP_METALLIC)
    mel.recompile_material(material)
    return material


def fresh_instance(name, parent):
    path = MATERIALS + "/" + name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.delete_asset(path)
    instance = tools.create_asset(name, MATERIALS, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(instance, parent)
    return instance


def build_instances(surface, plain, paint):
    for kind, (tile_cm, tint, rough_scale) in SURFACES.items():
        instance = fresh_instance("MI_" + kind, surface)
        for param, suffix in (("Colour", "D"), ("Normal", "N"), ("ORM", "ORM")):
            mel.set_material_instance_texture_parameter_value(instance, param, unreal.load_asset("%s/T_%s_%s" % (TEXTURES, kind, suffix)))
        mel.set_material_instance_scalar_parameter_value(instance, "TileCm", tile_cm)
        mel.set_material_instance_scalar_parameter_value(instance, "TintStrength", tint)
        mel.set_material_instance_scalar_parameter_value(instance, "RoughnessScale", rough_scale)
    for kind, (roughness, metallic, colour_scale) in PLAIN.items():
        instance = fresh_instance("MI_" + kind, plain)
        mel.set_material_instance_scalar_parameter_value(instance, "Roughness", roughness)
        mel.set_material_instance_scalar_parameter_value(instance, "Metallic", metallic)
        mel.set_material_instance_scalar_parameter_value(instance, "ColourScale", colour_scale)
    fresh_instance("MI_Paint", paint)
    log("built %d surface, %d plain and 1 paint instances" % (len(SURFACES), len(PLAIN)))


def main():
    import_textures()
    surface = build_surface_master()
    paint = build_paint_master()
    plain = build_plain_master()
    build_instances(surface, plain, paint)
    fresh_instance("MI_Terrain", build_terrain_master())
    unreal.EditorAssetLibrary.save_directory("/Game/Art", only_if_is_dirty=False, recursive=True)
    log("saved /Game/Art")


main()
