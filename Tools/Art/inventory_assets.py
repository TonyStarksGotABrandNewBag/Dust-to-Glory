"""Inventories what the project can draw with: every static mesh, skeletal mesh, material and texture
under /Game, with the facts that decide how it renders.

Static meshes: Nanite on or off, triangles and vertices at LOD 0, LOD count, material slots, bounds in
metres. Materials: parent chain to the master, blend mode, two-sided. Textures: size and compression.
Written to Saved/inventory.txt as a table per kind, grouped by folder, with totals, so the audit can say
what is Nanite and what is not instead of guessing. Headless, editor closed:

    UnrealEditor-Cmd.exe DustToGlory.uproject -run=pythonscript -script="Tools/Art/inventory_assets.py"
"""
import collections
import unreal

lib = unreal.EditorAssetLibrary
rows = collections.defaultdict(list)
totals = collections.Counter()


def folder_of(path):
    parts = path.split("/")
    return "/".join(parts[:4]) if len(parts) > 4 else "/".join(parts[:-1])


def static_mesh(path, mesh):
    nanite = mesh.get_editor_property("nanite_settings").enabled
    lods = mesh.get_num_lods()
    tris = mesh.get_num_triangles(0) if lods else 0
    verts = mesh.get_num_vertices(0) if lods else 0
    size = mesh.get_bounds().box_extent * 2.0 / 100.0
    slots = len(mesh.get_editor_property("static_materials"))
    totals["static meshes"] += 1
    totals["nanite meshes"] += 1 if nanite else 0
    totals["non-nanite triangles at LOD 0"] += 0 if nanite else tris
    rows[folder_of(path)].append("  SM  %-48s %s %8d tris %7d verts %d LODs %d mats %5.1fx%5.1fx%5.1f m" % (
        path.split("/")[-1], "NANITE" if nanite else "  mesh", tris, verts, lods, slots, size.x, size.y, size.z))


def material(path, asset):
    chain = []
    parent = asset
    while isinstance(parent, unreal.MaterialInstance):
        chain.append(parent.get_name())
        parent = parent.get_editor_property("parent")
    master = parent.get_name() if parent else "-"
    blend = str(parent.get_editor_property("blend_mode")).split(".")[-1] if isinstance(parent, unreal.Material) else "?"
    two_sided = parent.get_editor_property("two_sided") if isinstance(parent, unreal.Material) else False
    totals["materials"] += 1
    rows[folder_of(path)].append("  MAT %-48s master %-28s %s%s" % (path.split("/")[-1], master, blend, " two-sided" if two_sided else ""))


def texture(path, asset):
    w = asset.blueprint_get_size_x()
    h = asset.blueprint_get_size_y()
    comp = str(asset.get_editor_property("compression_settings")).split(".")[-1]
    totals["textures"] += 1
    totals["texture texels (millions)"] += (w * h) // 1000000
    rows[folder_of(path)].append("  TEX %-48s %5dx%-5d %s" % (path.split("/")[-1], w, h, comp))


def main():
    paths = sorted(p.split(".")[0] for p in lib.list_assets("/Game", recursive=True))
    for path in paths:
        data = unreal.EditorAssetLibrary.find_asset_data(path)
        kind = str(data.asset_class_path.asset_name)
        if kind == "StaticMesh":
            static_mesh(path, unreal.load_asset(path))
        elif kind in ("Material", "MaterialInstanceConstant"):
            material(path, unreal.load_asset(path))
        elif kind in ("Texture2D",):
            texture(path, unreal.load_asset(path))
        elif kind == "SkeletalMesh":
            totals["skeletal meshes"] += 1
            rows[folder_of(path)].append("  SK  %s" % path.split("/")[-1])
        else:
            totals["other: " + kind] += 1
    out = ["Dust to Glory asset inventory (what /Game can draw with)", ""]
    for key in sorted(totals):
        out.append("%-36s %d" % (key, totals[key]))
    out.append("")
    for folder in sorted(rows):
        out.append(folder + " (%d)" % len(rows[folder]))
        out.extend(sorted(rows[folder]))
        out.append("")
    target = unreal.Paths.project_saved_dir() + "inventory.txt"
    with open(target, "w", encoding="utf-8") as handle:
        handle.write("\n".join(out) + "\n")
    unreal.log("DTGInventory: wrote " + target)


main()
