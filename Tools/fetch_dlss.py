"""Installs NVIDIA's DLSS 4.5 plugins for UE 5.8 into Plugins/: Super Resolution, Ray Reconstruction,
Frame Generation and Reflex.

Downloaded from NVIDIA's developer site and used under NVIDIA's RTX SDK licence -- running this is
accepting it. The plugins are ~2 GB with debug symbols, so they are not committed; the project lists
them as optional plugins, and without them it builds and runs on Unreal's own upscaler (TSR).

    python Tools/fetch_dlss.py
"""
import hashlib
import pathlib
import shutil
import sys
import urllib.request
import zipfile

URL = ("https://developer.nvidia.com/downloads/assets/gameworks/downloads/secure/dlss/UE-DLSS-5.8/8.7.2/"
       "2026.07.21_UE5.8_DLSS4.5Plugin_v8.7.2.zip")
SHA256 = "caec541ce620ca60e455151e905e0e17cf01bd7337708ff5d3cae24fc9038bef"
PLUGINS = ["DLSS", "StreamlineNGXCommon", "StreamlineCore", "StreamlineDLSSG", "StreamlineReflex"]


def sha256_of(path: pathlib.Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def main() -> int:
    root = pathlib.Path(__file__).resolve().parents[1]
    archive = root / "ArtSource" / "NVIDIA" / "UE5.8_DLSS4.5Plugin_v8.7.2.zip"
    archive.parent.mkdir(parents=True, exist_ok=True)
    if not archive.exists() or sha256_of(archive) != SHA256:
        print("downloading", URL)
        request = urllib.request.Request(URL, headers={"User-Agent": "DustToGlory-fetch/1.0"})
        with urllib.request.urlopen(request, timeout=1800) as response, archive.open("wb") as out:
            shutil.copyfileobj(response, out)
        if sha256_of(archive) != SHA256:
            print("checksum mismatch; not installing", file=sys.stderr)
            return 1
    with zipfile.ZipFile(archive) as bundle:
        for plugin in PLUGINS:
            target = root / "Plugins" / plugin
            if target.exists():
                shutil.rmtree(target)
            prefix = "Plugins/%s/" % plugin
            members = [name for name in bundle.namelist() if name.startswith(prefix)]
            for name in members:
                destination = root / name
                if name.endswith("/"):
                    destination.mkdir(parents=True, exist_ok=True)
                    continue
                destination.parent.mkdir(parents=True, exist_ok=True)
                with bundle.open(name) as source, destination.open("wb") as out:
                    shutil.copyfileobj(source, out)
            print("installed %s (%d files)" % (plugin, len(members)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
