# Dust to Glory

An open-world life, driving and superhero sandbox set in a scaled-down South Carolina, built on Unreal
Engine 5.8. You start in Longs with an old car, a cheap apartment and nothing else, and go from there:
careers, businesses, ministries, racing, politics, crime — and three mutating powers that change as you
use them.

Present day, 1990s prices.

```
git clone https://github.com/TonyStarksGotABrandNewBag/Dust-to-Glory.git
cd Dust-to-Glory

# The simulation core builds and runs anywhere, with no engine installed:
cmake -S Tests -B Tests/build && cmake --build Tests/build
ctest --test-dir Tests/build --output-on-failure

# The game itself: open DustToGlory.uproject with Unreal Engine 5.8.
```

## What's in it

**The state.** Twenty-one real South Carolina towns at 1:25 scale between them, with full-size streets and
buildings inside each one: Longs, Myrtle Beach, Conway, Charleston, Columbia, Greenville, Hilton Head and
the rest, joined by I-95, I-26, I-20, I-85, I-77, US-17, US-501, SC-9, SC-31 and SC-905. Rivers and lakes
are crossed on bridges, the upstate rises into mountains, and the Atlantic is where it should be. Every
building can be walked into and every one does something.

**Cars.** Ninety-two fictional makes covering what's been sold in America, with generations going back to
the fifties. New showrooms, certified pre-owned, used lots, classic dealers, restorations and resto-mods,
and a locator service for anything not in stock. Manual or automatic, your choice, kept in your save.

Physics is a 240 Hz rigid body on Pacejka tires with a 500 Hz drivetrain: real gear ratios, differentials,
turbo spool, traction control and ABS, suspension that squats and rolls, and aero that matters at speed.
The engine note is synthesised from each engine's own firing order and the exhaust fitted, not a loop.

**A car parked at a house is a car you can buy.** Knock and haggle face to face, leave your number for
them to call, or text and email. Every negotiation is game theory: Nash bargaining face to face,
Rubinstein alternating offers over text, and owners who remember being messed about.

**Making a living.** Dozens of careers, DIY repair and flipping, buying businesses for cash or equity,
creating IP — songs, films, software, pharmaceuticals — and a stock market that reacts to all of it.
Reputation and social media are real currencies, and so is losing them.

**Faith and politics.** Found a ministry, preach, take tithes, run outreach — and keep your integrity, or
watch critics take it apart. Run for office from city council to president, propose laws and change the
rules everyone else plays by.

**Crime and consequence.** Police heat, arrest, a lawyer, a plea bargain or a trial, and a co-defendant
playing the prisoner's dilemma against you. Jail with work, classes, good-time credit and parole — or a
tunnel, a bribed guard, a crew outside, a medical transfer or your own powers. Escape and you're a
fugitive: no job will touch you, the manhunt cools only while you keep your head down, and getting caught
costs more than turning yourself in.

**Powers and fights.** Three powers at a time, chosen at the start, levelling and mutating unpredictably
with use. People who see you use them remember it, and enough of that wakes them up. Everyone has health:
hit a bystander and it's assault, put a wanted villain down and you're paid for it, lose and you wake up
at the hospital with a bill.

**Weather you feel.** The sun follows the clock and sets where it should; headlights come on at dusk.
Rain falls in three layers around you, leaning into the wind and into your own speed, so driving fast
lays it over towards you and it radiates from a point ahead. Lightning lights the sky and the thunder
arrives when its sound would really arrive -- a strike across the marsh is a roll eight seconds later.
Rain takes grip away from the tires while you watch it, and a storm takes more.

**People who talk back.** A passer-by is from somewhere, carries a problem, knows something and knows
other people. Ask about the town, their work, what they have been seeing, what is going around. Ask the
same thing twice and the answer gets shorter; keep at it and they walk off. Somebody who trusts you tells
you what is wrong in their life, and you can put it right. What they know has a price, split by Nash
bargaining, so a friend gives away what a stranger will not sell. And whatever you do in front of them
travels: word reaches the people they actually know, attributed and losing conviction at every mouth,
until the town has an opinion of you that nobody gave it.

**Three states.** South Carolina, Florida and North Carolina, picked from the front end. Florida is the
panhandle, the peninsula, the Gulf and the Atlantic, with US-1 out to Key West over open water. North
Carolina runs from the Outer Banks over Pamlico Sound to Mount Mitchell -- 2,037 m, the highest ground
east of the Mississippi -- and you start in Boone, up in the mountains, where the nearest city is an hour
away and every road out of town is a climb. The map generator takes a state file, so a fourth is data,
not code.

**Mods are just the data files.** Drop a folder in `Mods/` with any of them in it and the game uses yours
instead — including a whole new state.

## Before you build it

The simulation core — the rules, the physics, the economy, the game theory, all of it — is tested and
runs anywhere: 120 tests, green on Linux, macOS and Windows, and clean under the address and
undefined-behaviour sanitizers. That half you can trust.

The Unreal layer has never been compiled, because it was written somewhere without an engine installed.
It has been read closely, several times, and that found real things: a player spawning at the world
origin, a camera inside the car, pedestrians that could not move. But reading is not building, and the
first time you open `DustToGlory.uproject` you should expect to fix a few compile errors that a
compiler would have caught in a second. `Tests/check_headers.py` catches the one class of them a script
can — a simulation name used unqualified in an engine header — and runs in CI.

Nothing in `Sim/` depends on the engine, so a problem in `Game/` is always a local one.

## Repository layout

| Path | What it is |
| --- | --- |
| `Source/DustToGlory/Sim/` | The whole game, as engine-independent C++20. No Unreal headers. |
| `Source/DustToGlory/Game/` | The Unreal layer: pawns, world building, input, HUD, audio, AI. |
| `Content/Data/` | Every bit of game content, as JSON you can edit without a compiler. |
| `Tests/` | The core's test suite (CMake + CTest), and the content, service and header checks. |
| `Mods/` | Drop-in replacements for any data file, including whole new states. |
| `docs/` | Architecture, roadmap, content guide and audio notes. |

The split is the point: `Sim/` holds the rules and `Game/` only draws them, so the game is testable
without an engine and the handling is identical on every machine.

## Hardware

Built to run on an HP Omen with an RTX 5080 at full detail (hardware ray-traced Lumen, Nanite, virtual
shadow maps) and to scale down to a Mac on software Lumen. The world — terrain, roads, buildings, cars,
people — is generated at runtime from the data files rather than streamed from a baked map, so the
download stays small and there is no art pipeline to run.

## Content

Every make, model, business, ministry, team, person and phone number in the game is invented. The places
are real — that's the point of a map of South Carolina — but nothing in them is. See
[docs/CONTENT_GUIDE.md](docs/CONTENT_GUIDE.md).

## Documentation

- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) — how the simulation and the engine layer fit together
- [docs/ROADMAP.md](docs/ROADMAP.md) — what's done and what's next
- [docs/CONTENT_GUIDE.md](docs/CONTENT_GUIDE.md) — editing the data files
- [docs/AUDIO.md](docs/AUDIO.md) — how the engine sound is made
- [docs/SUNDOWN.md](docs/SUNDOWN.md) — the cheat codes, and the survival mode behind the last one
- [docs/REALISM.md](docs/REALISM.md) — the honest ledger against the photoreal target: what the code
  reaches on its own, and what needs the editor and art
- `python3 Tests/check_engine.py` — compiles the Unreal layer against a stand-in for the engine
  API, so a compiler checks it rather than a reviewer
