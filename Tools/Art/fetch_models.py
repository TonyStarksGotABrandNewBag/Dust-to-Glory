"""Downloads the CC0 models the world is furnished with: trees, ground cover and street furniture.

Every model here is from Poly Haven and is CC0 (public domain). Each lands as glTF with its textures in
ArtSource/PolyHavenModels/<id>/ (git-ignored); import_models.py turns them into Nanite static meshes
under Content/Art/Models/, and those are what get committed. Files already present with Poly Haven's
published MD5 are skipped.

    python Tools/Art/fetch_models.py [--resolution 2k]
"""
import argparse
import hashlib
import json
import pathlib
import sys
import urllib.request

API = "https://api.polyhaven.com"

# Game role -> Poly Haven model. The roles are what the game asks for; see Game/DTGProps.
MODELS = {
    "Pine": "pine_tree_01",
    "Fir": "fir_tree_01",
    "Palm1": "island_tree_01",
    "Palm2": "island_tree_02",
    "Palm3": "island_tree_03",
    "Jacaranda": "jacaranda_tree",
    "SmallTree": "tree_small_02",
    "Bermuda": "grass_bermuda_01",
    "GrassA": "grass_medium_01",
    "GrassB": "grass_medium_02",
    "Shrub1": "shrub_01",
    "Shrub2": "shrub_02",
    "Shrub3": "shrub_03",
    "Shrub4": "shrub_04",
    "Fern": "fern_02",
    "UtilityPoles": "modular_electricity_poles",
    "StreetLamp1": "street_lamp_01",
    "StreetLamp2": "street_lamp_02",
    "Hydrant": "fire_hydrant",
    "Barrier1": "concrete_road_barrier",
    "Barrier2": "concrete_road_barrier_02",
    "ChainlinkFence": "modular_chainlink_fence",
}


def fetch(url: str) -> bytes:
    request = urllib.request.Request(url, headers={"User-Agent": "DustToGlory-fetch/1.0"})
    with urllib.request.urlopen(request, timeout=900) as response:
        return response.read()


def md5_of(path: pathlib.Path) -> str:
    digest = hashlib.md5()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def get(entry: dict, target: pathlib.Path) -> int:
    """Downloads one file unless it is already there and intact. Returns its size, or -1 on a bad checksum."""
    target.parent.mkdir(parents=True, exist_ok=True)
    if target.exists() and md5_of(target) == entry["md5"]:
        return target.stat().st_size
    target.write_bytes(fetch(entry["url"]))
    if md5_of(target) != entry["md5"]:
        target.unlink()
        return -1
    return target.stat().st_size


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--resolution", default="2k")
    parser.add_argument("--only", nargs="*", help="model roles to fetch (default: all)")
    args = parser.parse_args()

    root = pathlib.Path(__file__).resolve().parents[2]
    out_dir = root / "ArtSource" / "PolyHavenModels"
    failures = 0
    total = 0
    for role, asset in MODELS.items():
        if args.only and role not in args.only:
            continue
        files = json.loads(fetch(f"{API}/files/{asset}"))
        try:
            gltf = files["gltf"][args.resolution]["gltf"]
        except KeyError:
            print(f"{role}: {asset} has no {args.resolution} glTF", file=sys.stderr)
            failures += 1
            continue
        folder = out_dir / asset
        size = get(gltf, folder / pathlib.Path(gltf["url"]).name)
        if size < 0:
            failures += 1
            continue
        for relative, entry in gltf.get("include", {}).items():
            part = get(entry, folder / relative)
            if part < 0:
                print(f"{asset}/{relative}: checksum mismatch", file=sys.stderr)
                failures += 1
            else:
                size += part
        # The glTF bundle leaves out the cut-out masks: leaf colour comes as a JPG with no alpha, so
        # without these every leaf card draws as a solid triangle. Poly Haven publishes them as
        # separate maps (leaves_alpha, twig_alpha, alpha), fetched alongside.
        for key, by_resolution in files.items():
            if not key.lower().endswith("alpha"):
                continue
            entry = by_resolution.get(args.resolution, {}).get("jpg")
            if not entry:
                continue
            part = get(entry, folder / "textures" / pathlib.Path(entry["url"]).name)
            if part < 0:
                print(f"{asset}/{key}: checksum mismatch", file=sys.stderr)
                failures += 1
            else:
                size += part
        total += size
        print(f"{role:15s} <- {asset} ({size / 1e6:.0f} MB)", flush=True)
    print(f"models: {total / 1e6:.0f} MB, {failures} failures")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
