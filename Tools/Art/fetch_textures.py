"""Downloads the photo-scanned surface materials the world is drawn with.

Every material here is from Poly Haven and is CC0 (public domain): free for any use, no attribution
required. The raw downloads land in ArtSource/PolyHaven/ (git-ignored); build_materials.py turns them
into Unreal assets under Content/Art/, and those are what get committed. Re-running this skips files
that are already present and match Poly Haven's published MD5.

    python Tools/Art/fetch_textures.py [--resolution 2k]
"""
import argparse
import hashlib
import json
import pathlib
import sys
import urllib.request

API = "https://api.polyhaven.com"

# Surface kind -> Poly Haven asset. The kinds are what the game asks for; see DTGSurface in Game/.
MATERIALS = {
    "Asphalt": "asphalt_02",
    "Sidewalk": "concrete_pavement",
    "Concrete": "concrete_wall_003",
    "Brick": "red_brick_03",
    "Stucco": "beige_wall_001",
    "WoodSiding": "brown_planks_03",
    "ShingleRoof": "roof_09",
    "MetalRoof": "corrugated_iron",
    "Grass": "leafy_grass",
    "Sand": "sand_01",
    "Dirt": "red_laterite_soil_stones",
    "ForestFloor": "forest_leaves_02",
    "PineBark": "pine_bark",
    "PalmBark": "palm_tree_bark",
    "Bark": "bark_brown_02",
}

# Poly Haven map name -> our suffix. "arm" packs AO, roughness and metalness into R, G and B, which is
# exactly Unreal's ORM convention. nor_dx is the DirectX (Unreal) normal-map handedness.
MAPS = {"Diffuse": "D", "nor_dx": "N", "arm": "ORM"}


def fetch_json(url: str) -> dict:
    request = urllib.request.Request(url, headers={"User-Agent": "DustToGlory-fetch/1.0"})
    with urllib.request.urlopen(request, timeout=60) as response:
        return json.load(response)


def md5_of(path: pathlib.Path) -> str:
    digest = hashlib.md5()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--resolution", default="2k")
    args = parser.parse_args()

    root = pathlib.Path(__file__).resolve().parents[2]
    out_dir = root / "ArtSource" / "PolyHaven"
    out_dir.mkdir(parents=True, exist_ok=True)

    total = 0
    failures = 0
    for kind, asset in MATERIALS.items():
        files = fetch_json(f"{API}/files/{asset}")
        for map_name, suffix in MAPS.items():
            try:
                entry = files[map_name][args.resolution]["jpg"]
            except KeyError:
                print(f"{kind}: {asset} has no {args.resolution} jpg {map_name}", file=sys.stderr)
                failures += 1
                continue
            target = out_dir / f"T_{kind}_{suffix}.jpg"
            if target.exists() and md5_of(target) == entry["md5"]:
                total += target.stat().st_size
                continue
            request = urllib.request.Request(entry["url"], headers={"User-Agent": "DustToGlory-fetch/1.0"})
            with urllib.request.urlopen(request, timeout=300) as response:
                target.write_bytes(response.read())
            if md5_of(target) != entry["md5"]:
                print(f"{target.name}: checksum mismatch", file=sys.stderr)
                target.unlink()
                failures += 1
                continue
            total += target.stat().st_size
            print(f"{target.name}  <- {asset} ({target.stat().st_size / 1e6:.1f} MB)")
    print(f"textures: {len(MATERIALS)} materials, {total / 1e6:.0f} MB, {failures} failures")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
