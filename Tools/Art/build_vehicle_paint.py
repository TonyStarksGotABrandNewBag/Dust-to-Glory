"""Makes, for each City Sample body, a paint instance the game can colour.

The pack's car paint picks its colour from a gradient by per-instance random, which an instanced
traffic mesh has and a plain actor never does, so every car the game dressed came out the gradient's
first colour (silver) whatever BaseColor was set to. Each instance made here is a child of the body's
own paint with that "Paint Variation" switch off, so BaseColor drives it (Game/DTGVehicle.cpp).

Run once after the pack has been added through the editor's Fab window (docs/HANDOFF.md), with the
editor closed:

    UnrealEditor-Cmd.exe DustToGlory.uproject -run=pythonscript -script="Tools/Art/build_vehicle_paint.py"

The instances land in /Game/CitySampleVehicles/DTG, which, like the pack, is not committed. The
commandlet does not echo unreal.log, so what was made is written to Saved/build_vehicle_paint.log.
"""
import unreal

mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
ROOT = "/Game/CitySampleVehicles"
OUT = ROOT + "/DTG"
# The same bodies as Game/DTGVehicleModels.cpp.
BODIES = [
    ("vehicle01_Van", "vehVan", "vehicle01"), ("vehicle02_Car", "vehCar", "vehicle02"),
    ("vehicle03_Car", "vehCar", "vehicle03"), ("vehicle04_Truck", "vehTruck", "vehicle04"),
    ("vehicle05_Car", "vehCar", "vehicle05"), ("vehicle06_Car", "vehCar", "vehicle06"),
    ("vehicle07_Car", "vehCar", "vehicle07"), ("vehicle08_Truck", "vehTruck", "vehicle08"),
    ("vehicle09_Van", "vehVan", "vehicle09"), ("vehicle10_Bus", "vehBus", "vehicle10"),
    ("vehicle11_Truck", "vehTruck", "vehicle11"), ("vehicle12_Car", "vehCar", "vehicle12"),
    ("vehicle13_Car", "vehCar", "vehicle13"),
]
REPORT = []


def say(text):
    REPORT.append(text)
    unreal.log("DTGPaint: " + text)


def paint_of(mesh):
    for slot in mesh.get_editor_property("static_materials"):
        if str(slot.material_slot_name) == "veh_carPaint":
            return slot.material_interface
    return None


def main():
    for folder, kind, vid in BODIES:
        mesh = unreal.load_asset("%s/%s/Mesh/SM_%s_%s_No_Wheel" % (ROOT, folder, kind, vid))
        if mesh is None:
            say("%s: no body mesh (is the pack in the project?)" % folder)
            continue
        parent = paint_of(mesh)
        if parent is None:
            say("%s: no veh_carPaint slot; its paint is left alone" % folder)
            continue
        name = "MI_DTG_Paint_" + folder
        path = OUT + "/" + name
        if unreal.EditorAssetLibrary.does_asset_exist(path):
            instance = unreal.load_asset(path)
        else:
            instance = tools.create_asset(name, OUT, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        mel.set_material_instance_parent(instance, parent)
        mel.set_material_instance_static_switch_parameter_value(instance, "Paint Variation", False)
        mel.update_material_instance(instance)
        say("%s: %s <- %s" % (folder, name, parent.get_name()))
    unreal.EditorAssetLibrary.save_directory(OUT, only_if_is_dirty=False, recursive=True)
    with open(unreal.Paths.project_saved_dir() + "build_vehicle_paint.log", "w", encoding="utf-8") as handle:
        handle.write("\n".join(REPORT) + "\n")


main()
