# Handoff — state of play

Written 19 October 2026, at the point where the simulation is complete and tested and the engine
layer has still never been compiled by Unreal.

## What is true right now

- **120 tests pass**, built with CMake in seconds, no engine required.
- **All four CI checks pass** (`check_content`, `check_services`, `check_headers`, `check_engine`).
- **28,378 lines of simulation**, **8,340 lines of engine layer**.
- Working tree clean, pushed to `TonyStarksGotABrandNewBag/Dust-to-Glory`.

## What is built

Three state maps (South Carolina, Florida, North Carolina) with real geography and real place names,
graded roads that never exceed 6%, bridges and overpasses. Towns with road grids and a role for every
building. Vehicles with a chassis simulation, tuning, synthesised engine audio from the real crank
layout, dealerships new/used/CPO/classic, restoration and resto-mod. Careers including ministry.
Crime, arrest, sentencing, jail, parole and escape. Pharmaceuticals as IP, engineered outbreaks, the
conspiracy to sell the cure. Reputation, markets, business ownership, politics to the presidency.
NPC minds with conversation, gossip and rumour spread. Powers that mutate. Weather and a day/night
clock. Vegetation by species and road markings to the US convention. Cheat codes dialled on the
phone. Sundown.

Details in `docs/ARCHITECTURE.md`, `docs/AUDIO.md`, `docs/REALISM.md`, `docs/SUNDOWN.md`.

## What is not done, in priority order

### 1. The engine layer has never been built by Unreal

This is the only thing that matters right now. Everything else is downstream of it.

`check_engine.py` compiles all 6,818 lines against `Tests/UnrealStub/` — a hand-written stand-in for
Unreal's API — at `-Wall -Wextra -Wshadow -Wconversion`. It is clean. That found and fixed four real
bugs, so it was worth doing, but it proves only that the code is well-formed C++ **against those
declarations**. Where they differ from the real engine it will disagree in both directions.

**What the first real build will probably throw, in descending order of confidence:**

- **UHT errors.** Unreal Header Tool has never run. It has its own rules the stub models not at all:
  `UPROPERTY` forms on `TObjectPtr`, `GENERATED_BODY` placement in `USTRUCT`s, `UFUNCTION`
  signatures, reflected types needing proper tags. This is an entire error class the stub is blind
  to and it is the top suspect.
- **Signature mismatches where the stub guessed.** Every one of those is marked `NOTE:` in
  `Tests/UnrealStub/`. The two least certain are `USynthComponent`'s `Init`/`OnGenerateAudio`
  contract (`Game/DTGEngineAudio.cpp`, `Game/DTGWeatherAudio.cpp`) and
  `UInputMappingContext::MapKey`'s return type and lifetime (`Game/DTGPlayerController.cpp`).
- **A missing module dependency** in `Build.cs`, which surfaces at link rather than compile.
- **`/Engine/Maps/Entry.Entry`** works as a default map in the editor but may need a real map for a
  packaged build.

**Estimate:** a few hours of iteration, not days. The stub has already cleared the trivial errors.

### 2. Nothing has ever run

No idea whether it crashes on `BeginPlay`, whether the world appears, or what the frame rate is. The
per-frame budget has been measured in simulation terms only (see below).

### 3. Materials

The entire world draws with one vertex-colour material. No asphalt, no bark, no glass, no car paint,
no wet-road reflection. This is the single largest gap between the current build and the look the
project is aiming at, and it is the one with the best return — the geometry underneath is already
correct, so a PBR material set moves everything a long way in one step. It needs a person in the
editor with assets. `docs/REALISM.md` has the full ledger.

### 4. Sundown's engine side

The mode is fully simulated and tested. What does not exist: spawning the Pale, their movement and
pack behaviour, the audio, and the lighting that makes a dark interior frightening rather than dim.
The economy is also not yet rewired for barter — cash is inert but the shops have not been closed.

## Performance, measured

From `Tools/profile_*.cpp`, on a 2.8 GHz Xeon, two cores:

| | |
|---|---|
| Whole state generated and playable | 24–46 ms |
| One streamed terrain chunk (67×67) | ~1.5 ms |
| `SurfaceAt` | 0.66 µs/sample |
| `RoadHeightAt` | 0.69 µs/sample |
| Vegetation per chunk | 0.7–1.4 ms, 23–70 plants |
| All road paint for a state, once at load | 1.6–4.9 ms |

Two chunks a frame is roughly 3 ms of a 16.7 ms budget. That headroom is what the visual work has to
fit inside.

## The build machine

Checked 19 October: the linked Windows machine `ci-ironman1` has Git and the Epic Games Launcher but
**no Unreal Engine and no Visual Studio 2022** (only VS 2010, which is useless here). Building needs
both — roughly 100–150 GB of installs. It looks like a work machine rather than the HP Omen with the
RTX 5080 that is the stated target. Confirm which machine before planning a build session.

Also unconfirmed: `DustToGlory.uproject` pins `EngineAssociation: 5.8`. Check what the Epic Launcher
actually offers and retarget if needed — it is a one-line change.

## What the project does not need

Worth knowing, because it is unusual: **there are no `.uasset` or `.umap` files anywhere, and none
are needed.** The default map is engine-provided (`/Engine/Maps/Entry.Entry`), the one material
referenced is engine-provided (`VertexColorMaterial`), and every input action and mapping context is
created at runtime with `NewObject`. There is no "first, author these assets in the editor" step —
clone and build.

## Suggested order from here

1. Get it compiling in Unreal on a machine that has the engine. Fix UHT and signature errors.
2. Get it running. Expect `BeginPlay` crashes; the world-building order has already been moved from
   `StartPlay` to `InitGame` once for exactly this reason.
3. Verify the measured budget survives contact with a real frame.
4. Materials and lighting, which is where the look actually comes from.
5. Sundown's engine side.
