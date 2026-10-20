"""Copies Epic's UE5 mannequins (Manny and Quinn, with their unarmed walk, jog and idle animations) out of
the engine's own template content into Content/Characters/Mannequins, where the game loads them for
people. Engine template content, used under the Unreal Engine EULA; 126 MB, so not committed -- each
machine runs this once. Without them, people are the generated box figures.

    python Tools/fetch_mannequins.py
"""
import pathlib
import shutil
import sys

ENGINE = pathlib.Path(r"C:\Program Files\Epic Games\UE_5.8")
SOURCE = ENGINE / "Templates" / "TemplateResources" / "High" / "Characters" / "Content" / "Mannequins"


def main() -> int:
    root = pathlib.Path(__file__).resolve().parents[1]
    target = root / "Content" / "Characters" / "Mannequins"
    if not SOURCE.is_dir():
        print("no mannequins at", SOURCE, file=sys.stderr)
        return 1
    if target.exists():
        shutil.rmtree(target)
    shutil.copytree(SOURCE, target)
    files = sum(1 for _ in target.rglob("*.uasset"))
    print("copied %d assets to %s" % (files, target))
    return 0


if __name__ == "__main__":
    sys.exit(main())
