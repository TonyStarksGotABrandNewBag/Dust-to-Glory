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

if Missing or Unknown:
    sys.exit(1)
print(f"services: ok ({len(Services)} services, all handled)")
