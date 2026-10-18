# Architecture

The game is split in two: a simulation that knows the rules and an engine layer that draws them. The rule
is one way — `Game/` may call into `Sim/`, `Sim/` never knows `Game/` exists.

```
Content/Data/*.json ──► Sim (rules, state, maths) ◄── Tests (CMake, no engine)
                             ▲
                             │ reads and calls, never the reverse
                             │
                        Game (Unreal: pawns, meshes, input, HUD, audio)
```

## Why

Three things fall out of the split, and they're the reason for it:

- **The game is testable without an engine.** 95 tests build in seconds with CMake and run anywhere,
  including CI on Linux, macOS and Windows. A physics change is a test, not a play session.
- **The handling is identical everywhere.** The chassis integrates at a fixed 240 Hz with its own rigid
  body; Unreal's physics never touches a car. The same inputs give the same lap on every machine.
- **Content is data.** Cars, careers, powers, furniture, laws, companies and the state map are JSON. A new
  make, a new career or a new town needs no code.

## The simulation (`Source/DustToGlory/Sim/`)

Engine-independent C++20, built by both UnrealBuildTool and CMake. No Unreal headers, no exceptions, no
RTTI, warnings as errors.

| Area | Files | What it holds |
| --- | --- | --- |
| Foundations | `SimCommon`, `Json`, `Math3`, `Clock`, `Economy` | Money, RNG, a JSON parser, vectors and quaternions in Unreal's conventions, the game calendar, the wallet. |
| Driving | `Tuning`, `Dynamics`, `ChassisSim`, `EngineAudio` | Vehicle setups and parts, the powertrain and tires at 500 Hz, the 240 Hz rigid body, and the exhaust synthesiser. |
| Cars as things you own | `Vehicles`, `Garage` | The catalog (brands, nameplates, generations, models), dealers, used lots, classics, restorations, valuations. |
| The world | `CityLayout`, `StatePlan` | How one town is laid out, and how twenty-one of them plus highways, rivers, lakes, mountains and venues make a state. |
| People | `NpcMinds`, `PrivateSales`, `Messages` | Minds that remember, hold a conversation, carry troubles and gossip, and can wake up; cars parked at houses; calls, texts and email. |
| Making a living | `Careers`, `Market`, `Hacking`, `Furniture` | Jobs and shifts, businesses, stocks, reputation, IP and royalties, hacking contracts, homes and furniture. |
| Belief and power | `Ministry`, `Politics`, `Powers` | Tithes, sermons, integrity and scandal; campaigns, offices and laws; three mutating powers. |
| Trouble | `Wanted`, `Justice`, `Conspiracy`, `Outbreak`, `Combat` | Police heat; courts, jail and the ways out; the engineered-outbreak scheme; illness as an SIR model; fights, health and bounties. |
| Game theory | `GameTheory` | Nash bargaining, Rubinstein offers, prisoner's dilemma, Cournot, Bertrand, the inspection game. |
| It all together | `GameState`, `GameStateSave` | The one object that owns everything, every action the player can take, and the save file. |

`GameState` is the whole API. The engine layer never reaches past it into a subsystem to change something.

### Game theory, concretely

John Nash's work isn't decoration here; it decides numbers the player feels:

- **Nash bargaining** sets the price when you haggle face to face, the sentence a prosecutor offers, what
  the state pays for a treatment, what a guard wants to look away, and what an insider wants to join you.
- **Rubinstein alternating offers** runs text haggling, where patience is leverage: `(1 − R) / (1 − P·R)`.
- **The prisoner's dilemma** is played for real by co-defendants and by everyone in a conspiracy, against
  a memory of how you've treated them before.
- **The inspection game** sets how often police patrol where a crime pays, and how often guards toss a cell.
- **Cournot and Bertrand** set what competitors do to your business's prices and volumes.

## The engine layer (`Source/DustToGlory/Game/`)

| File | Job |
| --- | --- |
| `DTGGameSubsystem` | Owns the one `GameState`, loads `Content/Data`, advances the clock, saves and loads. |
| `DTGGameMode` | Starts a session, builds the world, puts the player outside their apartment in Longs. |
| `DTGWorldBuilder` | Streams terrain chunks from the state's height field; places streets, highways with bridge decks, buildings, venues and water. |
| `DTGVehicle` | A pawn around `ChassisSim`: wheel ray casts into the Unreal world, body sweeps for collisions, wheels and body posed from the simulation. |
| `DTGEngineAudio` | A `USynthComponent` running the engine and road synthesisers on the audio render thread. |
| `DTGCharacter` | On foot: walking, what you're standing in front of, getting in and out, using powers. |
| `DTGPlayerController` | Enhanced Input built entirely in code: on-foot and driving contexts, manual shifting. |
| `DTGHud` | Instruments, minimap, notifications, and every menu — all drawn in code, no UMG assets. |
| `DTGMeshBuilder` | Every shape in the game, generated: buildings, cars, people, rooms, furniture. |
| `DTGInterior` | The room you are standing in when you walk through a door. |
| `DTGBody` | A jointed body with a walk: legs and arms swung at the pace the actor is really moving. |
| `DTGSky` | Sun, sky, fog and headlights, following the simulation's clock and weather. |
| `DTGTrafficDirector` | Traffic on the road graph, police who come looking, pedestrians carrying NPC minds, and cars parked outside houses. |
| `DTGEventDirector` | Drag races, derbies, monster jam and circuit races at the venues; heroes and villains roaming. |

### No art, either

Nothing in the repository is a mesh or a texture. `DTGMeshBuilder` generates every shape from the
simulation's numbers — a building from its footprint, floors and role; a car from its body style and real
dimensions — and all of it is drawn with the engine's vertex-colour material, so colour is carried in the
vertices. That is a floor rather than a ceiling: the shapes are the right size in the right places, which
is what makes them straightforward to swap for modelled art later.

Interiors work the same way. Rather than hollowing out a thousand shells, a room is built on demand in a
pocket of space far below the world and the player is moved into it; stepping back through the door puts
them where they were.

Units: the simulation works in metres, Unreal in centimetres. `DTGMetresToUnreal` is the only conversion,
and it lives in `DTGGameSubsystem.h`. Axes match Unreal throughout: X east, Y south, Z up.

### No map asset

There is no `.umap` to maintain. The game boots into the engine's empty Entry map and the game mode builds
everything: ground, roads, buildings, water, the player, the traffic. That's why the repository is almost
all source and JSON, and why a change to the state file changes the world with no editor work.

## The world, in two layers

`CityLayout` generates one town: a street grid, blocks, parks, lots and buildings with roles, addresses
and doors. `StatePlan` loads `state.json` and makes a state out of it — placing towns without overlapping
them or letting them fall in the sea, routing highways through towns and around the ones they don't serve,
joining crossings, and putting the four event venues clear of everything.

Terrain is a function, not a mesh: `StatePlan::SurfaceAt` gives the ground, the water and the road grade at
any point, built from the coast, the rivers, the lakes, the mountains and the level pads under towns.
`DTGWorldBuilder` samples it into chunks around the player and throws them away behind.

## Testing

`Tests/` builds the core with the same flags Unreal uses and runs 95 tests: physics plausibility (a car
accelerates, stops, corners, lands and rolls the way one should), the car economy, both state maps'
geography and road networks, private sales and messaging, careers, business, politics, courts and jail,
the ways out of jail and life as a fugitive, the outbreak scheme and the investigation that unpicks it,
fights and bounties, conversation and the way gossip crosses a town, rain and thunder, the
ordinary things a town sells, the spatial index against exhaustive
search, and a full save round trip. `Tests/check_content.py` parses every data file and
fails on a real brand name slipping in.

Three scripts cover what the tests cannot. `check_content.py` parses every data file. `check_services.py`
checks that every service a building offers is handled by the interface, and that every service with a
screen of its own actually builds one -- a service the interface does not recognise compiles, draws, and
tells the player the name of the thing it just failed to do. `check_headers.py` looks for simulation
names used unqualified in engine headers, where the `using namespace dtg::sim` at the top of the .cpp
has not reached yet; that error cannot be caught by building, because there is no Unreal Engine in CI.

The geometry has its own check outside the engine: `DTGMeshBuilder` compiles against a small stand-in for
Unreal's types and renders to an image, which is how the triangle winding was settled against Unreal's
face-normal convention and how the car silhouettes were judged.
