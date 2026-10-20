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
- **Leaf translucency.**
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
- **Night** (20 October): a moon (a second atmosphere light, there only once the sun is under), street
  lamps that come up with the dark (a 2000-lumen point light at every lamp head, scaled by the sky's
  night level, so a sunny street has no white pools), lit windows warmed to tungsten, and the exposure
  held two and a half stops under after dark -- left to itself the camera opened up until a moonlit
  street looked like an overcast afternoon. Photo mode adds `night-street` and `night-downtown` at 21:40,
  last, because the camera takes a while to open back up and the daylight shots came out washed out.
- **Frame rate** is logged per photo-mode shot: 31-58 fps by day at 1600x900 with everything on (RTX
  5070 Laptop), 26-28 fps at night (the moon's shadows, the lamps, the headlights). Headlights cast no
  shadows and only light up within 180 m of the camera: with every car's two beams shadowed, a night
  street ran at 10 fps. A shot that moves the clock settles for ten seconds, not five, because the
  lit windows' pipeline states compile and the camera re-adapts; only photo mode jumps the clock.

- **Real cars** (20 October): Epic's City Sample vehicles dress the simulation's cars when the pack is
  in the project (`Game/DTGVehicleModels`, which body for which body style). The pack's skeletal meshes
  are empty armatures; its art is Nanite static meshes authored in one space with the wheels in place,
  so one scaled rig carries the shell, the glass and each wheel, scaled to the simulation's wheelbase
  and placed with its tires on the ground the simulation found; the wheels spin, steer and ride the
  suspension travel from the chassis simulation. The pack's paint picks its colour from a gradient by
  per-instance random, which an actor never has, so `Tools/Art/build_vehicle_paint.py` makes a child
  instance per body with that off and the simulation's colour and finish drive it. Thirteen bodies,
  two of them static-mesh-only; the trailer is not used. Without the pack, cars are the boxes.

- **Facades** (20 October): sills that throw a shadow, lintels over masonry openings, shutters on half the
  houses, a line at every floor on the street buildings, a two-step cornice and a capped parapet, a
  downpipe at two corners, plant on the flat roofs, and dark built-up roofing instead of a slab of the
  wall's colour. Siding and stucco take a colour of their own per building from a Southern palette; the
  simulation colours by role, which painted every house the same beige. The wall shader weathers: rain
  streaks in patches and blotches, by surface (`Weathering` on each instance; none on the ground).

**Memory, measured** (20 October, `-DTGMemReport` at the car shot, RTX 5070 Laptop, 8 GB): 5.7 GB of
video memory in use at the shot, 7.7 GB at the peak during loading. It is not the car textures: the
texture streaming pool held 277 MB of its 1000, and the City Sample textures are virtual (320 MB of
pages). What it is: the whole map's 1160 building shells as one mesh per town -- 580 MB of vertex
buffers and 300 MB of ray-tracing structures, built at startup and never streamed -- Nanite's streaming
pool (528 MB), virtual shadow map pages (512 MB, now halved), Lumen's caches (600 MB, surface cache now
halved), and 2.1 GB of transient render targets at 1600x900 with DLSS. The first full tour with the cars
crashed the GPU at frame 6 with 6.7 GB of a 7.1 GB budget in use; ray tracing now sees Nanite's fallback
meshes (`r.RayTracing.Nanite.Mode=0`), after which the same tour ran, but at the peak the card is still
nearly full. Streaming the buildings by chunk, as the plants and props already are, is the real fix.

**Load time, measured** (20 October, `Static world:` in the log): the static world took 400 s to build,
381 of them in the building shells. Not the geometry (1.1 s): a procedural mesh section created with
collision cooks the whole component's collision again, so 191 sections cooked the map 191 times over.
The sections are now created without collision and cooked once at the end
(`ADTGWorldBuilder::CookBuildingCollision`): 1.3 s for the whole static world, a 14-view photo tour in two
minutes instead of ten, and the video memory peak down from 7.7 to 5.3 GB with it.

One engine ensure seen once per photo-mode run (`!Geometry->IsEvicted()` in RayTracing.cpp); harmless
so far, worth watching. The sun is six units, not a hundred thousand lux, so
every light in the game is scaled to it rather than physical; the lamp heads themselves do not glow.

**Lighting.** Done: Lumen GI and reflections, a sky atmosphere with the sun and moon driven by the
time-of-day clock, volumetric fog and clouds, and the weather driving the fog, the wet roads and the
lightning. The lights are tuned against each other rather than physical (see above).

**Nanite and foliage instancing.** Trees are procedural meshes built per chunk. That is affordable now
because there are tens per chunk, not thousands. Real forest density needs
`UHierarchicalInstancedStaticMeshComponent` with proper LODs and wind, which needs tree assets.

**Vehicle art.** Done with the City Sample bodies (above): glass, lights, grille, plates and interiors
come with them. Still generated: the box shell as the fallback, and the collision box the sweeps use,
which is the simulation's length and width rather than the body's. Headlights are the game's own
spotlights, not the model's lamps. The sim's fifteen body styles map onto the pack's eleven usable
shapes, so open-wheelers and monster trucks stay boxes.

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
