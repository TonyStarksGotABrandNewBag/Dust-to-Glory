# Realism

The target is the gameplay mockups: photoreal, present-day, somewhere in the Carolinas or Florida. This
document is the honest ledger against that target — what the code reaches on its own, what it cannot
reach without the Unreal editor, and what is deliberately not being attempted.

It exists because a milestone that says "realism: done" is worth nothing. Each line below is either
backed by a test, or named as a gap.

## What the simulation produces on its own

All of this is generated from the state data and verified by the CMake test suite, with no engine
involved. The offline renderer in the scratchpad draws it to check that the numbers add up to a picture.

| | Where | Checked by |
|---|---|---|
| Real geography: coastline, rivers, lakes, sounds, mountains | `Sim/StatePlan` | `TestState` — towns on land, non-overlapping, coast where it belongs |
| Roads that are cut and filled, not draped over hills | `StatePlan::GradeHighways` | every road in every state at or under 6% |
| Bridges and overpasses with their own deck profiles | `StatePlan::ComputeDecks` | roads never buried, never in the water |
| Trees and brush by species, placed by coast, elevation and water | `Sim/Vegetation` | `TestVegetation` — stable in world space, off the road, out of town, species suit the ground |
| Road paint to the US convention | `Sim/RoadMarkings` | `TestMarkings` — on the pavement along its whole length, yellow and white mean what they mean |
| Towns with a road grid, lots, and a role for every building | `Sim/CityLayout` | `check_services.py` — every building offers something |
| Weather, time of day, rain geometry, thunder | `Sim/Clock`, `Game/DTGWeatherFx` | verified by offline rasterisation |
| Engine note from crank layout and fitted parts | `Sim/EngineAudio` | `docs/AUDIO.md` |

### Vegetation

Longleaf and loblolly pine inland, live oak through the older country and the towns, cabbage palm and
palmetto within about a kilometre of the coast, bald cypress where there is standing water within a short
walk, and scrub where the ground is steep or thin. Nothing grows on a carriageway, in open water, or on a
town street — only the parks are planted inside a town.

The placement is deterministic in world space rather than per chunk. This matters more than it sounds:
terrain streams in chunks that are built, released and built again as the player drives, so anything
keyed to the chunk would shift every time you drove past it. `TestVegetation` asks for the same ground
twice and in four overlapping pieces and requires the same trees back.

Cost is about 23 plants per chunk in South Carolina, 43 in Florida and 70 in North Carolina — roughly
1,300 to 3,400 triangles — at 0.7 to 1.4 ms to choose and 0.1 ms to build.

### Road paint

Yellow separates traffic going opposite ways and white separates traffic going the same way, which is the
thing a player who drives these roads every day would notice first if it were wrong. An undivided highway
gets a double yellow centre, white edge lines and broken white lane lines; a divided one gets no yellow
down the middle at all — a yellow left edge on each carriageway, white on the right. Town junctions get
crosswalk ladders and stop bars.

Stripes break only where the road's own polyline bends or its deck changes slope, so a straight run is a
single quad. That is 29,498 stripes and 4.9 ms for North Carolina rather than 144,274 and 20.4 ms, with
a 120 m cap so no triangle is so large that it never frustum-culls.

## What needs the editor

None of the following can be faked in C++, and attempting to would produce something that looks worse
than honest flat shading. Each needs a person in the Unreal editor with assets.

**Materials.** Partly done, 19 October. Every generated surface is tagged with what it is made of
(`Game/DTGSurface.h`) and drawn with that kind's material. The materials are built headless from code
(`Tools/Art/build_materials.py`) over 15 photo-scanned CC0 texture sets from Poly Haven
(`Tools/Art/fetch_textures.py`). They are projected from world space (triplanar), so the procedural
meshes need neither UVs nor tangents.

- **Done:** asphalt, sidewalk, concrete, brick, stucco, wood siding, shingle and metal roofs, and park
  grass.
- **Done:** terrain blending grass, beach sand and red clay by the existing class colours.
- **Done:** glass, chrome, rubber, and car paint whose roughness and metalness follow `PaintFinish`.
- **Done:** a glossy water surface.

Still missing:
- **Bark and leaves.** Plants are still one flat-coloured section. `PlantTri` has no trunk/foliage tag yet.
- **Clear-coat paint.** UE 5.8's Python API does not expose the clear-coat pins.
- **Leaf translucency, emissive lights and lit windows.**
- **Wet roads.** Nothing drives a wetness parameter from the weather yet.
- **Real shapes.** Every shape is still a box, and textures do not change that.

**Rendering, 19 October.** Now on in this project:

- **Shader Model 6.** Nanite and virtual shadow maps were silently off before, because only SM5 was targeted.
- **Hardware ray-traced Lumen,** with hit-lit reflections, and Nanite geometry in the ray tracing scene.
- **Volumetric clouds** and thinner clear-weather fog.
- **A post-process grade** (in `ADTGSky`).
- **NVIDIA DLSS 4.5:** Quality-mode super resolution, Ray Reconstruction, and auto frame generation.
  `Tools/fetch_dlss.py` installs it, and the plugins are optional.
- **Street furniture:** CC0 models placed by `Sim/StreetFurniture` (poles, lamps, hydrants, deck barriers).

- **Facades with depth:** windows and doors sit in real reveals cut through the wall, not on it; the
  glass carries an interior-mapped room (lit or dark, fading with depth) under ray-traced reflections.
- **Walls:** a second rotated texture copy blended by world noise kills the tiling, and vertex alpha
  carries height above ground for grime and damp at the foot of every wall.
- **Street trees** on every lawned block (palms near the coast, live oaks inland), placed and tested
  in `Sim/StreetFurniture`.
- **Golden hour** in photo mode: the same street and highway at 19:05, the light the game is meant for.

- **Power lines** between the utility poles (`Sim/StreetFurniture` `WiresIn`, tested), three sagging
  lines a span, with a gap wherever a pole was skipped.
- **Grass clumps** on every lawn (`GroundCoverIn`, tested), scanned tufts scaled to about a metre.
- **Rain wets the roads** and **windows light up after dark**: `ADTGSky` drives `MPC_DTG` (Wet, Night).
- **People are Epic's mannequins** (`Tools/fetch_mannequins.py`) with idle, walk and jog clips matched
  to ground speed; the box figure stands in when the assets are not there.
- **Frame rate** is logged per photo-mode shot: 32-67 fps at 1600x900 with everything on (RTX 5070).

Still box geometry: cars. One engine ensure seen once in photo mode (`!Geometry->IsEvicted()` in
RayTracing.cpp); harmless so far, worth watching.
Not checked: the frame rate with all of it on.

**Lighting.** Lumen GI and reflections, a sky atmosphere driven by the existing time-of-day clock, and
volumetric fog for the morning haze the mockups lean on. The simulation already produces sun angle and
weather state; nothing consumes them for lighting yet.

**Nanite and foliage instancing.** Trees are procedural meshes built per chunk. That is affordable now
because there are tens per chunk, not thousands. Real forest density needs
`UHierarchicalInstancedStaticMeshComponent` with proper LODs and wind, which needs tree assets.

**Vehicle art.** Cars are built from tapered boxes. The chassis simulation, the engine audio and the
tuning are all real; the shells are placeholders. Needs modelled bodies with glass, lights, grille,
badges and interiors.

**Character art.** Pedestrians and the player are capsules with simple meshes. Needs skeletal meshes and
animation sets — idle, walk, run, enter and exit a vehicle, and the conversation poses the NPC minds
already drive.

**Audio mixing.** The engine note is synthesised and correct. Tyre noise, wind, impacts, ambience and
the attenuation and reverb settings around them are editor work.

## Deliberately not attempted

- **Real brands.** Imitation makes and models only, as the brief asks. No real logos or badges.
- **Photogrammetry or scanned terrain.** The map is real geography at a compressed scale — real place
  names and real relative positions, not real elevation data.
- **Superelevation.** Carriageways are flat in cross-section; roads are not banked through curves. The
  paint and the deck assume this, so banking would mean changing both.
- **A physical median.** A divided highway is one graded ribbon with a painted median rather than two
  separate carriageways with a gap. At the map's compressed scale, two carriageways with a true median
  would be wider than the towns they pass.

## How this is checked without an engine

Three things, in increasing cost:

1. **The CMake test suite** (111 tests). Geometry, placement rules, determinism, conventions, budgets.
2. **Offline rasterisation.** A software renderer in the scratchpad draws the world's own data from a
   driver's eye. This is how the vegetation and the paint were checked, and it is why the renderer is
   worth keeping: it caught that the terrain and the planting agreed, and it is the only thing that
   answers "does this look like the Southeast".
3. **Mutation testing.** Every new test above was confirmed by breaking the thing it guards and watching
   it fail. A test that has never been seen to fail is not evidence of anything.

4. **A compiler reading the engine layer.** `Tests/check_engine.py` compiles all 6,800 lines of
   `Source/DustToGlory/Game` against `Tests/UnrealStub` — a stand-in for Unreal's API, just large
   enough to parse the project — with `-Wall -Wextra -Wshadow -Wconversion`. Every compile error
   found in the engine layer before this existed was found by a person reading the code, which is
   the wrong tool for the job: four got through and were only caught on a later pass.

   It is a syntax and type check and nothing more. It does not run, it draws nothing, and where the
   stub's declarations are wrong it will disagree with the real engine in both directions. A clean
   run is necessary, not sufficient.

   It was verified by planting each of the historical failures and confirming it catches them: a
   misspelled member, an unqualified `dtg::sim` name in an engine header, `FMath::Max(1,
   FMath::CeilToInt(SomeDouble))` (which fails because the double overload returns `int64`), an
   `int64` → `int32` narrowing, and a `Super::` call that the real base class does not have.

What none of this checks is how the world looks *in Unreal*, or whether the stand-in's declarations
match the real engine. The first real build will still find things — it just will not be spending
its time on typos.
