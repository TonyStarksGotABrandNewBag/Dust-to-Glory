#!/usr/bin/env python3
"""Engine headers must spell simulation names out in full.

`using namespace dtg::sim;` lives at the top of the engine .cpp files, which is after their headers
have already been parsed. So a bare `Money` or `GameState` in a Game/*.h compiles nowhere, and this
repository has no way to find that out -- there is no Unreal Engine here to try it. Twice now a bare
name has reached a commit and been caught by reading rather than by building, which is exactly the
kind of thing a script should be doing instead.

This reads every name the simulation exports and looks for any of them used unqualified in an engine
header. It is deliberately blunt: a false positive is a header that should be clearer anyway.
"""
import re
import sys
from pathlib import Path

Root = Path(__file__).resolve().parent.parent
SimDir = Root / "Source/DustToGlory/Sim"
GameDir = Root / "Source/DustToGlory/Game"

# What dtg::sim exports: types, enums and free functions declared at namespace scope.
Exported: set[str] = set()
for Header in sorted(SimDir.glob("*.h")):
    Text = Header.read_text()
    Exported |= set(re.findall(r"^\t(?:struct|class|enum class)\s+(\w+)", Text, re.M))
    Exported |= set(re.findall(r"^\tusing\s+(\w+)\s*=", Text, re.M))
    Exported |= set(re.findall(r"^\tconstexpr\s+\w+\s+(\w+)\s*=", Text, re.M))

# Names that are also ordinary English words in comments, or Unreal's own, are matched in code only.
def CodeOnly(Text: str) -> str:
    Text = re.sub(r"/\*.*?\*/", "", Text, flags=re.S)
    Text = re.sub(r"//[^\n]*", "", Text)
    return re.sub(r'"(?:[^"\\]|\\.)*"', '""', Text)

Problems: list[str] = []
for Header in sorted(GameDir.glob("*.h")):
    Text = CodeOnly(Header.read_text())
    if "using namespace dtg::sim" in Text:
        Problems.append(f"{Header.name}: has a using-directive in a header, which leaks into every "
                        f"file that includes it")
        continue
    for Line, Content in enumerate(Text.splitlines(), start=1):
        # A forward declaration names the type without needing it qualified in the same way.
        if re.match(r"\s*(?:class|struct|enum)\s+\w+;", Content):
            continue
        for Name in re.findall(r"(?<![\w:.>])([A-Z]\w+)", Content):
            if Name in Exported and f"dtg::sim::{Name}" not in Content:
                Problems.append(f"{Header.name}:{Line}: '{Name}' is a dtg::sim name used unqualified "
                                f"in a header -- write 'dtg::sim::{Name}'")

for Problem in Problems:
    print(f"headers: {Problem}")
if Problems:
    sys.exit(1)
print(f"headers: ok ({len(Exported)} simulation names, all qualified where engine headers use them)")
