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

**Done 19 October, on `ci-ironman1` with UE 5.8.3 and MSVC 14.44.** `DustToGloryEditor Win64
Development` builds and links with zero warnings after three rounds. What the real build actually
threw, for calibrating the guesses below:

- UHT: one error. A `UPROPERTY` named `Layers` shadowed `AActor::Layers` (renamed `StreakLayers`).
- `USynthComponent` has only an `FObjectInitializer` constructor; both synth subclasses needed one.
  `Init`/`OnGenerateAudio` were guessed right. `MapKey` was fine.
- C4458 on locals named `Role` and `Player`, which hide engine members.
- A bare `Rect` was ambiguous with SlateCore's global `struct Rect;`.
- No missing module dependencies. The stub now reproduces the constructor and the `Rect` cases.
  That stub change is untested here: this machine has no GCC/Clang, so `check_engine.py` skips.

The original assessment follows.

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

**First run, 19 October, `-game` on `ci-ironman1`:** the state loads (1,160 buildings, 21 towns) and
the map comes up in about 0.4 s, with no `BeginPlay` crash. The HUD, powers, minimap and player all
appear. Three things were wrong and are fixed:

- `VertexColorMaterial` lives in `/Engine/EngineDebugMaterials/`, not `/Engine/EngineMaterials/`, so the
  world drew with the checkerboard fallback. Path and cook list corrected.
- Traffic spawned cars onto junctions that already had one, and each stacked pair blew its chassis up
  to NaN. Spawns now skip an occupied junction.
- `ChassisSim`'s blow-up guard cleared velocity after position had already been integrated from it,
  so a NaN position never recovered. It now rolls back the whole sub-step
  (`ChassisRecoversFromANumericalBlowUp`, mutation-tested). `ADTGVehicle` also guards the collision
  response and logs the first rollback per car.

After the fixes, 90 seconds ran with no ensures and no rollbacks. Not yet checked: driving, frame rate,
night, weather, Sundown. The original note follows.

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
| Static world in Unreal (streets, 1160 shells, venues, water, one collision cook), RTX 5070 laptop | 1.3 s |
| Full 14-view photo tour, `-DTGPhoto`, including load | 2 min |

Two chunks a frame is roughly 3 ms of a 16.7 ms budget. That headroom is what the visual work has to
fit inside.

## The build machine

Checked 19 October: the linked Windows machine `ci-ironman1` has Git and the Epic Games Launcher but
**no Unreal Engine and no Visual Studio 2022** (only VS 2010, which is useless here). Building needs
both — roughly 100–150 GB of installs. It looks like a work machine rather than the HP Omen with the
RTX 5080 that is the stated target.

Update, same day: Kenny confirmed `ci-ironman1` is the build machine. The Epic Launcher offered and is
installing **UE 5.8.3**, so `EngineAssociation: 5.8` stands — no retarget. Visual Studio 2022
Community (17.14, game and desktop C++ workloads, Windows SDK 10.0.22621) is being installed. VS 2022
rather than 2026 because Epic's own 5.8 build farm uses VS 2022 17.14 with the 14.44 toolset, and the
14.51 toolset VS 2026 now ships has known 5.8 compile failures. Frame-rate numbers from this machine
will not represent the Omen.

## What the project does not need

**Superseded 19 October.** The project now has art: `Content/Art/` holds imported textures and
materials as `.uasset` files tracked by Git LFS. Run `git lfs install` and `git lfs pull` after
cloning.

- **Without the LFS files** the game still runs. Every surface falls back to the engine's
  vertex-colour material and logs a warning once per kind.
- **Rebuilding the art from source:**
  1. `python Tools/Art/fetch_textures.py` downloads the CC0 textures into `ArtSource/`, which is
     git-ignored.
  2. Run `Tools/Art/build_materials.py` through `UnrealEditor-Cmd -run=pythonscript`. It rebuilds
     every material; with `DTG_MATERIAL_STAGES=window` (or `surfaces`) in the environment, only
     those. It keeps `MPC_DTG`'s existing parameters: the materials reference them by id, and a
     rebuilt collection leaves every material built against the old one failing to compile.
- **Fab packs (City Sample Vehicles, City Sample Crowds, Megascans):** not committed (`.gitignore`),
  gigabytes. They come in through the editor's own Fab window, not the launcher: Window -> Fab, sign in
  (it opens an Epic device-activation page in the system browser), My Library, the + on each tile.
  Each download link carries a token that expires in minutes, so queue them one at a time -- fourteen at
  once left everything behind the first two packs with a 403. On this PC the launcher's "Add To Project"
  is dead: its project list is empty because the engine was never registered in the registry
  (launcher offers "Fix Now", one UAC prompt, never accepted). `Content/CitySampleVehicles` is read by
  `Game/DTGVehicleModels`; without it cars are the generated boxes. After the pack is in, run
  `Tools/Art/build_vehicle_paint.py` through `UnrealEditor-Cmd -run=pythonscript` (editor closed): it
  makes the paint instances the game colours; without them every car is the pack's silver.
- **Still true:** the default map is engine-provided, and the input actions are created at runtime.

## Suggested order from here

1. ~~Get it compiling in Unreal on a machine that has the engine. Fix UHT and signature errors.~~ Done.
2. Get it running. Expect `BeginPlay` crashes; the world-building order has already been moved from
   `StartPlay` to `InitGame` once for exactly this reason.
3. Verify the measured budget survives contact with a real frame.
4. Materials and lighting, which is where the look actually comes from.
5. Sundown's engine side.
