#!/usr/bin/env python3
"""Generates the .generated.h stand-ins that let the engine layer be compiled without Unreal.

Unreal Header Tool writes a .generated.h next to every header holding a UCLASS, and GENERATED_BODY()
expands -- through CURRENT_FILE_ID and __LINE__ -- to a macro that file defines. One of the things it
declares is `Super`, the typedef for the class's base, which the engine layer uses in about twenty
places.

This reproduces that mechanism rather than working around it, because the alternative (defining
Super to something fixed, or deleting the uses) would stop the compiler checking the thing worth
checking: that Super::BeginPlay() actually exists on the base the class really derives from.

Run from anywhere; writes into Tests/UnrealStub/.
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
GAME = ROOT / "Source" / "DustToGlory" / "Game"
OUT = ROOT / "Tests" / "UnrealStub"

# "class DUSTTOGLORY_API ADTGFoo : public AActor" / "struct FDTGBar"
TYPE_RE = re.compile(
    r"^\s*(class|struct)\s+(?:DUSTTOGLORY_API\s+)?(\w+)\s*(?::\s*public\s+(\w+))?\s*$"
)
BODY_RE = re.compile(r"^\s*GENERATED_(?:USTRUCT_)?BODY\s*\(\s*\)")

HEADER = """// Stand-in for Unreal Header Tool output -- GENERATED, do not edit.
// Rebuild with Tests/make_unreal_stub_headers.py. See Tests/UnrealStub/CoreMinimal.h for why.
#pragma once

#undef CURRENT_FILE_ID
#define CURRENT_FILE_ID {file_id}
"""


def main() -> int:
    if not GAME.is_dir():
        print(f"no engine layer at {GAME}", file=sys.stderr)
        return 1
    OUT.mkdir(parents=True, exist_ok=True)

    written = 0
    for header in sorted(GAME.glob("*.h")):
        text = header.read_text(encoding="utf-8")
        if f'{header.stem}.generated.h' not in text:
            continue
        lines = text.splitlines()
        file_id = f"{header.stem}_h"
        out = [HEADER.format(file_id=file_id)]

        # Walk forward: remember the most recent type declaration, and when a GENERATED_BODY() turns
        # up, bind that line number to that type's base.
        pending = None
        found = 0
        for number, line in enumerate(lines, start=1):
            match = TYPE_RE.match(line)
            if match:
                pending = match
                continue
            if BODY_RE.match(line) and pending is not None:
                base = pending.group(3)
                name = pending.group(2)
                if base:
                    out.append(
                        f"// {name} : public {base}\n"
                        f"#define {file_id}_{number}_GENERATED_BODY \\\n"
                        f"\tpublic: \\\n"
                        f"\tusing Super = {base}; \\\n"
                        f"\tusing ThisClass = {name}; \\\n"
                        f"\tstatic UClass* StaticClass();\n"
                    )
                else:
                    out.append(
                        f"// {name} (no base)\n"
                        f"#define {file_id}_{number}_GENERATED_BODY \\\n"
                        f"\tpublic: \\\n"
                        f"\tusing ThisClass = {name};\n"
                    )
                found += 1
                pending = None

        (OUT / f"{header.stem}.generated.h").write_text("\n".join(out) + "\n", encoding="utf-8")
        written += 1
        print(f"  {header.stem}.generated.h: {found} reflected type(s)")

    print(f"wrote {written} generated-header stand-ins")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
