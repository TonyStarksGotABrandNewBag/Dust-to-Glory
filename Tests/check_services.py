#!/usr/bin/env python3
"""Every service a building offers must be handled by the HUD.

The simulation names the services (ServicesForRole) and the interface acts on them by name. A service
the HUD does not recognise is a menu row that does nothing, and nothing else catches that: it compiles,
it draws, and it tells the player the name of the thing it just failed to do. This checks both
directions -- every service is handled, and every name the HUD handles is a real service.
"""
import re
import sys
from pathlib import Path

Root = Path(__file__).resolve().parent.parent
Sim = (Root / "Source/DustToGlory/Sim/CityLayout.cpp").read_text()
Hud = (Root / "Source/DustToGlory/Game/DTGHud.cpp").read_text()

Block = Sim[Sim.index("std::vector<std::string> ServicesForRole"):]
Block = Block[:Block.index("\n\t}")]
Services = set(re.findall(r'"([^"]+)"', Block))

Handled = (set(re.findall(r'What == TEXT\("([^"]+)"\)', Hud))
           | set(re.findall(r'Service == TEXT\("([^"]+)"\)', Hud))
           | set(re.findall(r'\{TEXT\("([^"]+)"\), Amenity::', Hud)))
Prefixes = set(re.findall(r'Service\.StartsWith\(TEXT\("([^"]+)"\)', Hud))

Missing = sorted(s for s in Services
                 if s not in Handled and not any(s.startswith(p) for p in Prefixes))
Unknown = sorted(h for h in Handled if h not in Services)

for Name in Missing:
    print(f"services: '{Name}' is offered by a building but the HUD does nothing with it")
for Name in Unknown:
    print(f"services: the HUD handles '{Name}', which no building offers -- a typo, or a stale name")

# A service that opens a screen of its own is named in one list and builds its rows in another. If
# those drift, the player gets a screen with nothing on it but "Back", or a service that silently
# falls through to a flavour message with a whole menu sitting unused behind it.
Block = Hud[Hud.index("	static const TCHAR* const Listed[] = {"):]
Block = Block[:Block.index("};")]
Listed = set(re.findall(r'TEXT\("([^"]+)"\)', Block))

Rows = Hud[Hud.index("TArray<ADTGHud::FServiceRow> ADTGHud::ServiceRows() const"):]
Rows = Rows[:Rows.index("\nvoid ADTGHud::RunServiceOption")]
Produces = set(re.findall(r'What == TEXT\("([^"]+)"\)', Rows))

for Name in sorted(Listed - Produces):
    print(f"services: '{Name}' opens a screen of its own but builds no rows for it")
for Name in sorted(Produces - Listed):
    print(f"services: '{Name}' builds rows that nothing opens")

if Missing or Unknown or (Listed - Produces) or (Produces - Listed):
    sys.exit(1)
print(f"services: ok ({len(Services)} services, all handled; {len(Listed)} with screens of their own)")
