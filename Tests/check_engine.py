#!/usr/bin/env python3
"""Compiles the engine layer against a stand-in for Unreal, so a compiler reads it instead of me.

Source/DustToGlory/Game has never been built by an actual engine. Every compile error found in it so
far was found by a person reading the code, which is a bad way to find compile errors: four got
through that way and were only caught on a later pass.

This points a real compiler at all of it, using the declarations in Tests/UnrealStub. What it
catches is what a compiler catches: misspelled members, wrong argument types and counts, missing
overrides, unqualified dtg::sim names, shadowed members, unused parameters.

What it does not catch is anything about how Unreal behaves, or whether the stub's declarations
match the real engine's. A clean run here is necessary, not sufficient. The first real build will
still find things; this just means it will not be spending its time on typos.

Exit code 0 when clean. Run: python3 Tests/check_engine.py [--warnings-as-errors]
"""
import pathlib
import shutil
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
GAME = ROOT / "Source" / "DustToGlory" / "Game"
STUB = ROOT / "Tests" / "UnrealStub"
GENERATOR = ROOT / "Tests" / "make_unreal_stub_headers.py"

# -Wconversion earns its place: int64 -> int32 narrowing is the error that got through review four
# times here, and it is invisible without it. -Wno-float-conversion goes with it because double ->
# float is not a mistake in this codebase, it is the boundary between a double-precision simulation
# and an engine whose API is float, and warning on all several dozen of those would train us to
# ignore the output.
WARNINGS = [
    "-Wall",
    "-Wextra",
    "-Wshadow",
    "-Wconversion",
    "-Wno-float-conversion",
    "-Wno-sign-conversion",
]


def compiler() -> str | None:
    for name in ("g++", "clang++", "c++"):
        found = shutil.which(name)
        if found:
            return found
    return None


def main() -> int:
    strict = "--warnings-as-errors" in sys.argv

    tool = compiler()
    if tool is None:
        print("engine: SKIPPED (no C++ compiler on PATH)")
        return 0
    if not GAME.is_dir():
        print(f"engine: no engine layer at {GAME}", file=sys.stderr)
        return 1

    # The .generated.h stand-ins encode each class's base, so they must be rebuilt whenever a class
    # gains, loses or changes one. Cheap enough to do every run rather than remember to.
    regenerate = subprocess.run(
        [sys.executable, str(GENERATOR)], capture_output=True, text=True
    )
    if regenerate.returncode != 0:
        print("engine: could not rebuild the generated-header stand-ins", file=sys.stderr)
        print(regenerate.stderr, file=sys.stderr)
        return 1

    sources = sorted(GAME.glob("*.cpp"))
    if not sources:
        print("engine: no sources found", file=sys.stderr)
        return 1

    lines = 0
    for source in sources:
        lines += len(source.read_text(encoding="utf-8").splitlines())

    errors: list[str] = []
    warnings: list[str] = []
    for source in sources:
        result = subprocess.run(
            [
                tool,
                "-fsyntax-only",
                "-std=c++20",
                *WARNINGS,
                "-I", str(STUB),
                "-I", str(ROOT / "Source" / "DustToGlory"),
                str(source),
            ],
            capture_output=True,
            text=True,
        )
        for line in result.stderr.splitlines():
            if ": error:" in line:
                errors.append(line)
            elif ": warning:" in line:
                warnings.append(line)

    for line in errors:
        print(f"  {line}")
    for line in warnings:
        print(f"  {line}")

    if errors:
        print(f"engine: {len(errors)} error(s) in {len(sources)} files")
        return 1
    if warnings and strict:
        print(f"engine: {len(warnings)} warning(s) in {len(sources)} files (treated as errors)")
        return 1
    if warnings:
        print(f"engine: compiles, but with {len(warnings)} warning(s)")
        return 0

    print(
        f"engine: ok ({len(sources)} files, {lines} lines compiled against the Unreal stand-in, "
        f"no errors or warnings)"
    )
    print("        NOTE: a syntax and type check only -- it proves nothing about engine behaviour.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
