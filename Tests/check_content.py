#!/usr/bin/env python3
"""Checks that every file in Content/Data parses.

The data files use // line comments, which JSON proper does not allow; the game's own parser accepts
them (see Sim/Json.cpp), so this strips them the same way before parsing.
"""
import json
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
DATA = ROOT / "Content" / "Data"

REQUIRED = [
    "economy.json", "vehicles.json", "parts.json", "careers.json",
    "powers.json", "furniture.json", "companies.json", "laws.json", "state.json",
]
# Every state file, whatever it is called: the front end offers whatever is in the folder, so a new
# one that does not parse is a menu entry that fails when it is picked.
REQUIRED += sorted(p.name for p in DATA.glob("state-*.json"))


def check_state(name: str, data: dict) -> list:
    """A state file's own internal consistency, which JSON parsing cannot see.

    A highway can name a city as a waypoint instead of giving coordinates. If that name is not a city
    in the same file the route quietly loses a point, and the road goes somewhere else -- there is no
    error anywhere, just a highway that misses the town it is named for.
    """
    problems = []
    towns = {city["name"] for city in data.get("cities", [])}
    starts = [city["name"] for city in data.get("cities", []) if city.get("start")]
    for highway in data.get("highways", []):
        for point in highway.get("route", []):
            if isinstance(point, str) and point not in towns:
                problems.append(f"{name}: {highway['name']} routes through '{point}', "
                                f"which is not a city in this state")
    if len(starts) != 1:
        problems.append(f"{name}: {len(starts)} cities marked as the start; there must be exactly one")
    outline = data.get("outline", [])
    coast = data.get("coast", [])
    if len(coast) == 2 and not (0 <= coast[0] < coast[1] <= len(outline)):
        problems.append(f"{name}: coast {coast} is not a range inside an outline of {len(outline)} points")
    return problems


def strip_comments(text: str) -> str:
    """Removes // comments that are not inside a string."""
    out = []
    in_string = False
    escaped = False
    index = 0
    while index < len(text):
        char = text[index]
        if in_string:
            out.append(char)
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == '"':
                in_string = False
            index += 1
            continue
        if char == '"':
            in_string = True
            out.append(char)
            index += 1
            continue
        if char == "/" and index + 1 < len(text) and text[index + 1] == "/":
            while index < len(text) and text[index] != "\n":
                index += 1
            continue
        out.append(char)
        index += 1
    return "".join(out)


def main() -> int:
    failures = 0
    for name in REQUIRED:
        path = DATA / name
        if not path.exists():
            print(f"missing: {name}")
            failures += 1
            continue
        try:
            value = json.loads(strip_comments(path.read_text(encoding="utf-8")))
        except json.JSONDecodeError as error:
            print(f"{name}: {error}")
            failures += 1
            continue
        if name.startswith("state"):
            problems = check_state(name, value)
            for problem in problems:
                print(problem)
            failures += len(problems)
            if problems:
                continue
        count = len(value) if isinstance(value, (list, dict)) else 1
        print(f"{name}: ok ({count} top-level entries)")

    # Nothing in the game world carries a real brand, person or team name, so flag the obvious slips.
    banned = re.compile(r"\b(Chevrolet|Chevy|Ford|Toyota|Honda|Nissan|Hyundai|Kia|Volkswagen|Audi|BMW|Mercedes|Porsche|Ferrari|Lamborghini|Dodge|Jeep|Subaru|Mazda|Lexus|Infiniti|Genesis|GMC|Cadillac|Buick|Tesla|Rivian|Scion)\b", re.IGNORECASE)
    for path in sorted(DATA.glob("*.json")):
        for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), start=1):
            match = banned.search(line)
            if match:
                print(f"{path.name}:{number}: real brand name '{match.group(0)}' -- every make in this game is fictional")
                failures += 1
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
