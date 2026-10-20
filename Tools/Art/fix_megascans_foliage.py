"""Gives the Megascans plants the project's own masked material.

Through the Fab plugin's foliage master (M_MS_Foliage) these drew as near-black stems in this project,
under every renderer setting tried -- hardware ray tracing off, Nanite off, Ray Reconstruction off, the
master's own parameters zeroed one by one -- while the project's M_DTG_ModelMasked over the very same
textures draws them as grass. The cause in the Fab master was not found. So each plant mesh's material
becomes a child of M_DTG_ModelMasked carrying the pack's colour, normal and opacity (the ORT mask)
textures, saved beside the pack under /Game/Fab/DTG and assigned to the mesh, and the game's code
need know nothing about it.

Run after the plants have been added through the editor's Fab window (docs/HANDOFF.md), editor closed:

    UnrealEditor-Cmd.exe DustToGlory.uproject -run=pythonscript -script="Tools/Art/fix_megascans_foliage.py"

Neither the pack nor /Game/Fab/DTG is committed. Saved/fix_megascans_foliage.log says what was done.
"""
import unreal

mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
PLANTS = "/Game/Fab/Megascans/Plants"
OUT = "/Game/Fab/DTG"
MASTER = "/Game/Art/Materials/M_DTG_ModelMasked"
REPORT = []


def say(text):
    REPORT.append(text)
    unreal.log("DTGFoliage: " + text)


def is_fab_foliage(material):
    parent = material
    while isinstance(parent, unreal.MaterialInstance):
        parent = parent.get_editor_property("parent")
    return parent is not None and parent.get_name() == "M_MS_Foliage"


def instance_for(theirs, master):
    name = "MI_DTG_" + theirs.get_name()
    path = OUT + "/" + name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        ours = unreal.load_asset(path)
    else:
        ours = tools.create_asset(name, OUT, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(ours, master)
    for theirs_param, ours_param in (("BaseColorTexture", "BaseColorTexture"), ("NormalTexture", "NormalTexture"), ("Mask", "OpacityTexture")):
        texture = mel.get_material_instance_texture_parameter_value(theirs, theirs_param)
        if texture:
            mel.set_material_instance_texture_parameter_value(ours, ours_param, texture)
        else:
            say("  %s has no %s" % (theirs.get_name(), theirs_param))
    mel.update_material_instance(ours)
    return ours


def main():
    master = unreal.load_asset(MASTER)
    if master is None:
        say("no %s; Tools/Art/import_models.py builds it" % MASTER)
        return
    made = {}
    meshes = 0
    for path in sorted(unreal.EditorAssetLibrary.list_assets(PLANTS, recursive=True)):
        p = path.split(".")[0]
        mesh = unreal.load_asset(p)
        if not isinstance(mesh, unreal.StaticMesh):
            continue
        slots = list(mesh.get_editor_property("static_materials"))
        changed = False
        for slot in slots:
            theirs = slot.material_interface
            if not isinstance(theirs, unreal.MaterialInstance) or not is_fab_foliage(theirs):
                continue
            key = theirs.get_path_name()
            if key not in made:
                made[key] = instance_for(theirs, master)
                say("%s -> %s" % (theirs.get_name(), made[key].get_name()))
            slot.material_interface = made[key]
            changed = True
        if changed:
            mesh.set_editor_property("static_materials", slots)
            unreal.EditorAssetLibrary.save_loaded_asset(mesh, only_if_is_dirty=False)
            meshes += 1
    unreal.EditorAssetLibrary.save_directory(OUT, only_if_is_dirty=False, recursive=True)
    say("%d materials made, %d meshes reassigned" % (len(made), meshes))
    with open(unreal.Paths.project_saved_dir() + "fix_megascans_foliage.log", "w", encoding="utf-8") as handle:
        handle.write("\n".join(REPORT) + "\n")


main()
