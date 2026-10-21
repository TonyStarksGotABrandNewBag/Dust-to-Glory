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
- **Clear-coat paint on the generated cars.** UE 5.8's Python API does not expose the clear-coat pins
  of `M_DTG_Paint`; the City Sample bodies bring their own clear coat, so this only shows on the box
  fallback.
- **Leaf translucency** on the Poly Haven trees (the Megascans grasses and palm shrubs have it).
- **Shapes.** Cars are real bodies and plants real scans; buildings are still boxes with trim, furniture
  a handful of models, people mannequins.

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

- **Megascans on the street** (20 October): the curb is Quixel curb stones laid end to end along every
  block edge, each its own length, standing two centimetres proud of the kerb quad (verified in the
  `kerb` photo-mode shot, knee high along the kerb); a Quixel trash can stands on the sidewalk at the
  corner across from the hydrant (`Sim/StreetFurniture` `Bins`, tested and mutation-checked: the corner
  rule and the setback each fail their own mutant). The palmettos are the Quixel Alexandra palm shrubs
  and the lawn clumps the Kikuyu and wild grasses, where imported, with the Poly Haven scans as the
  fallback (`Game/DTGPlantModels`). Through the Fab plugin's foliage master these drew as near-black
  stems in this project under every setting tried -- hardware ray tracing off, Nanite off, Ray
  Reconstruction off, the master's parameters zeroed one at a time -- while the project's
  `M_DTG_ModelMasked` over the same textures draws them as grass (a `-DTGMatSwap` A/B, since removed).
  `Tools/Art/fix_megascans_foliage.py` re-materials the plant meshes with that; the cause in the Fab
  master was not found, so its wind and subsurface are lost until it is.

- **People are City Sample crowd characters** (20 October) when that pack is in the project: MetaHuman
  heads (six a sex) with their scanned hair as grooms bound to the face, a synthesized body, and a top,
  bottom and shoes from the pack's wardrobe, chosen by the person's seed and the same every time; an
  unseen base mesh plays the pack's idle, walk and brisk-walk clips and every visible piece follows its
  pose (`UDTGBody::BuildCrowd`). The mannequins are the fallback, the box figure after that. Each person
  is coloured by seed: the synthesized skin takes a melanin and a redness seed (the face the same two as
  the arms), and each garment its colour from a street palette through the pack's clothing master
  (`A_CrowdColor_main`); the shoes, which did not show until they were given a colour, show. The clips'
  paces are measured from their root motion at load rather than guessed (walk 140 cm/s for both sexes,
  brisk 207 for the women's clip and 244 for the men's), so the feet hold the ground at any speed.
  Verified in three photo-mode shots the same day: three people with different tops, trousers, shoes
  and skin, two of them walking the pavement at 160 cm/s on the walk clip at rate 1.14, feet planted.

- **Pedestrians walk** (20 October). They never had: the traffic director set
  `bRunPhysicsWithNoController` after spawning, and a character picks its walking movement mode at spawn
  only when that flag is already set, so with no controller all forty stood at 0 cm/s for ever (photo
  mode's listing of every character showed it). `SetDefaultMovementMode()` after the flag fixes it. They
  spawn on the pavement ring round their block, not at its centre. Errands and strolls alike go by the
  pavements (`PavementRoute`, `Sim/Pavement.h`): round the ring of the block they are on to the corner
  facing the goal, straight over the road to the same corner of the next block, block by block, then up
  the front path to the door. An errand goes to the nearest building of the kind the mind names -- it
  went to the first in the state, usually in another town -- and is seen through rather than rethought
  every few seconds, unless it takes longer than walking it at 1 m/s plus ten seconds. The test walks 300
  random errands across the biggest town: no leg enters a building, crosses a lawn, or crosses a road
  except straight over at a corner, and none walks more than 120 m beyond the grid distance (the worst
  is 71). Eleven mutants of the routing each fail it. In the game, with every second pedestrian sent on
  an errand by a temporary hack and a census of all forty every five seconds for three minutes: nobody
  ever inside a building, a lawn only on a front path 2-4 m from a door, everyone else on the pavement or
  crossing at a corner. The census also showed a sixth of them at a time stood nose to nose with somebody
  coming the other way along the same line (strolls did it too); Unreal's crowd avoidance took that from
  6.7 of 40 to 0.1. Not verified: errands at night, when the scripted minds all head home (the census
  forced daytime ones instead), and a crowd at one door, which avoidance jostles rather than queues.
  Photo mode's `person` and `walker` shots are framed when they come up, ahead of where the nearest
  person on the street will be when the shot is taken, with the eye kept off the lots (from inside a shell the one-sided walls are not there); `walker`
  waits up to 30 s for someone walking, and both now see the plain-character pedestrians, which they
  never did before: both searches iterated the project's own character class, which only the player is.
  Their cameras often land in the street-tree row 3.3 m in from the kerb: the `walker` one did in three
  runs out of three on 20 October, the person on the pavement behind the trunk.

- **Nobody can shove a car by walking into it** (20 October). They could: a car sweeps its body on the
  DTGBodySweep channel and backs out of whatever blocks the sweep, and a person's capsule blocked it, so
  walking into a parked car pushed it down the street while the player walked on through the space it
  left (the car's body answered the Pawn channel with overlap). Now a capsule, the player's and the
  pedestrians', ignores DTGBodySweep, and the car's body blocks Pawn. `-DTGPushCheck` proves it the way
  a test would: it stands the player a stride off the nearest standing car, waits four seconds to see
  how far the car creeps by itself, walks the player into it for four more, and logs the verdict. Before
  the fix: the car moved 12.10 m and the player ended inside it. After: 0.16 m, less than the 0.24 m it
  drifted on its own, and the player stopped at the side. Not fixed, and noticed by that baseline: a
  standing car creeps at about 6 cm/s on its own (the starter car, on the spawn street); a parking brake
  in the chassis simulation is the likely answer. Not verified: what happens to a pedestrian a moving car
  drives into now that the body blocks them (the character's own depenetration decides).

## Environmental realism milestone 2: Longs as an inhabited place, 21 October

Kenny's brief: coordinated property-level generation, roads and terrain that meet, buildings and
properties as archetypes, layered coastal-plain vegetation, the towers traced, and one camera judged
against photographs at day, night and rain; preserve the gameplay, simulation, weather, saves and the
performance work. Every change is a rule in Sim/ with a mutation-tested test, drawn by the world
builder; nothing is scattered by hand. The views are `longs-noon`, `longs-golden`, `longs-night`,
`longs-rain` (one eye on the shoulder of the main road) and `longs-above` (the crossroads from 45 m).

**Properties (Sim/Lots).** Each building in a crossroads community gets its lot back from the block it
stands in and, in the lot's own axes (along the road, out from it), a gravel or concrete drive from the
road's edge to a parking pad beside the house on the side with room, a walk from the pad to the door,
a mailbox at the road's edge on the far side of the drive facing the road, foundation shrubs a stride
apart along the front wall clear of the door, a chain-link fence (Poly Haven `SM_ChainlinkFence`, laid
end to end) round a third of the lots on their sides and back from behind the pad, a shed on half of
them in the back corner that faces into the block (never the side road of a corner lot), and the
culvert's two headwalls where the drive crosses the ditch. `EveryCrossroadsLotHasADriveToTheRoad`,
`LotsAreStableInWorldSpace` and the mutations M8 (a drive that starts off the road), M9 (shrubs three
metres out) and M13 (sheds on the road side) each fail exactly their test.

**Roads and ground.** A crossroads community's streets are county roads: a solid double yellow centre
line and white edge lines (`CrossroadsStreetsAreMarkedAsCountyRoads`, mutation M12), the asphalt's
edge wandering a hand's width where it has crumbled (a strip of quads every two metres with the
ground's noise), a sand shoulder that is the ground itself coloured sand out to where the ditch
begins, and a ditch along every street in the ground the cars and people stand on
(`StatePlan::DitchBesideStreets`, flat shoulder to 1.4 m, a 45 cm swale to 4.6 m;
`CrossroadsStreetsHaveDitches`, mutation M10), with the terrain at a vertex every metre where a
crossroads community is so the swale holds its shape. Road paint had been laid 3.5 cm above the ground
under a slab 4 cm above it, so no street had ever shown its lines; it sits 7 cm up now. The ground
under the pines and oaks is needles and leaf litter (the terrain's dirt weight by the trees in the
chunk), not lawn.

**Buildings.** A house is one of four kinds decided with the plan (`BuildingPlan::HomeStyle`): a brick
ranch with a hip roof (`FDTGMesh::AddHipRoof`, new) and no shutters, a siding ranch with a porch and
shutters, a double-wide (a long 8.5 m box, small windows, a low metal lid, steps and no porch) and a
two-storey farmhouse (full-width porch with posts, metal gable, tall windows). A crossroads community's
stores have metal gables, not a town's flat roof and parapet. `HomesComeInKinds` and mutations M14
(every house a siding ranch) and M15 (double-wides as deep as houses).

**Vegetation.** The country round a crossroads community is pine woods for 260 m whatever the stand
noise says (`CrossroadsWoodsM`), and the woods have a floor: an understory of bush, fern, palmetto and
low shrub on a 4.5 m grid where the canopy is dense and thin where it is open
(`CrossroadsStandInTheWoodsWithAnUnderstory`, mutation M11). The species-mix test now counts trees,
not the floor under them. Yard trees keep five metres from the street, out of the ditch.

**Towers.** Traced: the world builder draws every building of the state in one component at any
distance, with no culling; the towers in Longs' views are North Myrtle Beach's oceanfront row (560 m
at the state's 25:1 compression, 6 to 14 floors since the first audit) and Myrtle Beach's hub. The
woods round Longs now stand between the street and them at eye level; from above, and from the
highway, they remain, as they should at this scale. Myrtle Beach and North Myrtle Beach are unchanged.

**What the captures showed, and what they changed (21 October).** Inspected, not assumed; each
view is in `Saved/Photos/m2` (first pass) and `Saved/Photos/m2f` (after the fixes below):

- `longs-lot` (a double-wide from 16 m up): sand drive from the road's edge to a pad beside the house,
  the household's car on the pad nose to the house, a concrete walk to the door, the mailbox at the
  edge on the far side of the drive, foundation shrubs, the neighbours' sheds and chain-link. The
  culvert's headwalls stay under the drive. Also in the frame: a traffic car on its roof on the
  shoulder beside a lamp it had knocked over. Traffic had always spawned at a crossroads with a random
  heading and U-turned to get going, across the shoulder; the ditch in the ground turned that into a
  rollover. A car now spawns facing its first leg (`ADTGTrafficDirector::UpdateTraffic`). That is a
  fix to the evident cause, not a proven one: the second pass shows no car on its roof, which is one
  sample.
- `longs-above` (the crossroads from 45 m): county-road markings, the ragged edge, drives, pads,
  sheds in the back corners, chain-link round a third of the lots (from above, a standing fence and
  its shadow read as a ladder; it is standing), litter under the trees. Twin-lantern street lamps every
  32 m along the blocks were a town's furniture on a county road: a crossroads community now keeps the
  lamp nearest each junction and none along the block (`CrossroadsTownsLightOnlyTheirJunctions`;
  mutation M16 puts 16 of Longs' 28 lamps back along the blocks and fails exactly that test). The
  night view is darker between the junctions for it, which is what Longs is.
- The four shoulder views: the sand shoulder had been coloured out to where the ditch begins, three
  metres of bright sand with a red line at its far edge. The line is the terrain material: it weights
  five class colours with Gaussians (`Tools/Art/build_materials.py`), and the colour a triangle
  interpolates between grass and sand passes nearest its rock and lake-bed classes, so the metre
  between a sand vertex and a grass vertex reads as clay. The sand is now the first 0.8 m off the
  asphalt (one vertex row at the 1 m spacing) and the ditch is grassed; the clay seam is one vertex
  wide and remains. Removing it means the material reading a mask channel instead of classifying
  colours, which is a material-asset change and needs the editor.

**Measured (1600×900, `-DTGGpuProfile`, the main view's GPU time; DLSS Quality against native with
`r.NGX.DLSS.Enable 0, r.ScreenPercentage 100`).**

| View          | DLSS fps | DLSS GPU ms | Native fps | Native GPU ms |
|---------------|---------:|------------:|-----------:|--------------:|
| longs-noon    | 41       | 23.8        | 35         | 23.8          |
| longs-golden  | 29       | 23.5        | –          | –             |
| longs-night   | 28       | 25.5        | 25         | 29.0          |
| longs-rain    | 29       | 22.3        | 26         | 25.0          |
| longs-lot     | 29       | 23.0        | –          | –             |
| longs-above   | 36       | 17.7        | –          | –             |

Noon's GPU time did not move with the internal resolution, so that view is bound by work that does
not scale with pixels: the non-Nanite shadow cost the fidelity audit measured at 6 to 8 ms is the
known candidate. The fps column is one sample at the capture and moves run to run: the second pass
(after the fixes above, DLSS) gave noon 38, night 29, lot 30 and above 37 against the first pass's
41, 28, 29 and 36, so read the GPU column and treat the fps as ±3. The 14 crossroads chunks drew 1 to 12 lots each, under 2,000 built vertices and
under 800 fence panels per chunk (`Lots in chunk` in the log). The VSM marking-queue warning fires
once in each pass, at frame 2, while the state's 1,156 building shells are the only thing in the
shadow map before the first chunk arrives, and not again; no ray-tracing budget warning in either.

**Fab and other assets the slice uses, by path.** Quixel Megascans (via Fab): `Alexandra_Palm_ukgkcilia`
(the palmetto understory), `Kikuyu_Grass_wiljearqx` and `Wild_Grass_vlkhcbxia` (ground grass),
`Modular_Curb_Kit_xbttdihdw` (towns only; a crossroads community has no kerb), `Trash_Can_ucynedrfa`,
`Old_Wooden_Pole` (utility poles). Epic City Sample: the vehicles and the crowd bodies and clothes.
Poly Haven (CC0, fetched by `Tools/Art/fetch_models.py` into `/Game/Art/Models`): `ChainlinkFence`
(modular_chainlink_fence), `Pine`, `Shrub2`, `Fern`, `Shrub4`, `StreetLamp1`/`StreetLamp2`
(street_lamp_01), `UtilityPoles`. The houses, sheds, mailboxes, drives, pads, walks and culvert
headwalls are built geometry (`FDTGMesh`), not assets. `Fine_American_Road` and the `Yellow_Road_Line`
decal are in the project and unused: the road is the built slab with the painted lines, and swapping
the surface is a material job for the editor.

**Still wrong, in the order it shows.**

1. The pines read as bare at eye level: the `Pine` model's crown is sparse and the stand is thin. The
   woods belt round a crossroads community (`CrossroadsWoodsM`, density floor 0.45) does not read as
   woods from the road; a loblolly stand wants trees four to six metres apart, which over the belt is
   about 20,000 more instances, and that is a measured performance change, not a one-line one.
2. The stores along the crossroads are one archetype: a long metal-gabled box with a row of windows.
   The four house kinds vary; the commercial frontage does not yet (a gas canopy, a church with a
   steeple, a brick storefront with a sign band are the next three).
3. People walk the carriageway and the yards where there is no pavement, and three men in the night
   view are the same body in the same clothes in step. Crowd variety and the rural walking routes are
   outside this milestone and still open.
4. Utility poles carry wires along the roads (section 2 of the chunk) but no drop to any house, and
   there is no mailbox post reflector, no bins, no vehicles in the yards beyond the one car.
5. The red clay seam at the shoulder (above), and the dirt texture itself is red, where Horry County's
   soil is grey-brown sand: both are material-asset changes.
6. The far ground is bare beyond the chunk reach (`ViewDistanceM` 900), and North Myrtle Beach's row
   stands 560 m away at the state's 25:1 compression, so it is in every eastward frame. Real OSM or
   county parcel data for Longs is still not in the project.

The milestone's own test: Kenny's brief says it is complete only when the rendered area visibly
resembles an inhabited South Carolina neighbourhood. From the road at noon it reads as a county road
through a rural crossroads with lots, drives and mailboxes, under a southern sky; it does not yet read
as Longs, for reasons 1 and 2 above, and nobody should say otherwise until the pines stand thick.
## World-generation audit: why Longs did not look like Longs, 20 October

Kenny's second brief: the environment looked generated and not like South Carolina; audit the generator
before touching anything, name the files and rules, then the smallest set of changes. Every cause below
is in the code, not in the assets.

**Towers in a rural community.** Longs is defined right: `Content/Data/state.json` makes it `Rural`, grid
2, and `CityLayout.cpp`'s `ZoneOf` gives Rural every block as `Outer`, one or two floors. The towers in
its views belong to North Myrtle Beach and Myrtle Beach: the state is real geography compressed 25:1
(`"scale": 25`), which puts North Myrtle Beach 560 m from Longs and Myrtle Beach 1.2 km, inside the
900 m chunk radius and on every horizon, and the `Beach` style's zone rule made every inner block of a
resort town `Downtown`, which is 6 to 22 floors. Photo mode's `downtown` view then aimed at the nearest
tower, which was never in Longs. Fixed in the rule: a Beach town stands towers (6 to 14 floors, condominiums)
on its oceanfront row only, motels (2 to 4) on the row behind, one and two storeys everywhere else; the
state plan tells each town which side the sea is on (`StatePlan::CoastSideOf`, `CityConfig::CoastSide`).
`BeachTowersStandOnTheOceanfrontOnly` holds it, and fails when the rule is widened to every row.

**Buildings repeat.** `BuildingPlan` carries a role, a footprint, floors and one colour; `DTGMeshBuilder`
has one facade generator with one window module laid out in regular rows (`AddWindowRow`), a wall
colour from a small palette per role (`WallColourFor`) and a few dressings. Nothing in the plan says
era, builder or type, so two houses differ only in colour and size. Not changed here: it is the facades
brief (conway-facades), which also needs the Beach towers to read as condominiums rather than offices.

**Flat, featureless ground.** The coastal plain is flat (Longs is 10 m above the sea), and the state's
`verticalScale` of 3 cannot make it otherwise; the featurelessness is the terrain chunk's one lawn
material (`M_DTG_Terrain`) with no ground-cover change, ditches or shoulders along the roads. Not
changed here: the roads-ground brief.

**Roads five lanes wide.** One `RoadWidth` of 18 m served every town (`StatePlan.cpp`, `CityConfig`),
with 4 m sidewalks, so a crossroads community's street was the hub's. Now `RoadWidthFor(CityStyle)`: a
crossroads community 10 m (two lanes and shoulders), a town 12, a city or resort strip 14, the hub 18;
the town's span, its highway gates and its layout all take it. `RoadWidthFollowsTheTownsStyle` holds
it. No markings are drawn anywhere yet (the marking plan exists; roads-ground brief).

**Trees oversized, repetitive, in the way.** The plant heights are right (pines 13 to 27 m, live oaks 8
to 16, `Vegetation.cpp`) but the models are not those sizes: `DTGPlantModels::TransformFor` scales a
model to the plant's height, and the Poly Haven pine scan is 8.6 m tall, so it is drawn at two to three
times its size with bark and needles to match; the Jacaranda and SmallTree scans stand in for live oak
with low wide crowns that, at 8 to 16 m, reach over the road; the Megascans Alexandra palm shrub stands
in for the 6 to 13 m cabbage palm at three times its size. And every town had a boulevard: a street
tree every 11 m along every block with a lawn (`StreetFurniture.cpp`, `TreeSpacingM`), 3.3 m in from the
kerb, with crowns over the carriageway and in front of the facades, while `Vegetation` kept out of
every town except its parks (`bAvoidTowns`), so no yard had a tree and no lot was wooded. Changed: a
crossroads community has no street trees (`CrossroadsTownsHaveNoStreetTrees`) and its lots are wooded
instead, at a third of the forest's density, off the pavement and three metres clear of every building
(`YardAt`, `CrossroadsLotsAreWooded`); both mutation-tested. Not changed: the model sizes, which need
pine, live oak and palm models of their true height (the sc-setting brief).

**No property boundaries.** Lots are implicit (a footprint and a setback); there are no driveways,
fences, mailboxes or a lawn-to-woods edge, and the sidewalk ring goes round every block of every style.
Not changed here (roads-ground and sc-setting briefs).

**Beyond the chunk radius there is nothing**: 900 m out, the ground is the lawn-coloured sea slab and no
tree stands, which is why every long view ends in a bare horizon. Not changed here (the static-chunks
brief is where a far ring belongs).

**What was changed, the smallest set that moves Longs:** towers only on a resort town's oceanfront;
streets by style (10 m in Longs); no street trees and wooded yards in crossroads communities (Longs went
from 53 yard trees to 205 with the density floor); crossroads buildings single-storey and at most 26 m
long; crossroads blocks lawn to the road's edge, no sidewalk ring, no kerb stones; every window its own
lit lottery (the material always had one, but every window sat in the same cell, so a whole town lit or
went dark together); rain as lit streaks half a metre long (unlit, they were black at noon and white at
night under the physical exposure); the engine cloud material's coverage and storm look driven by the
weather, so rain falls out of an overcast and not out of blue; four acceptance views of the start town
from one eye on the road's shoulder (`longs-noon`, `longs-golden`, `longs-night`, `longs-rain`, the
clock and weather forced per shot), and photo mode holds its view target every frame, because at noon
something in the game took it back and the shot was the player's. Seven rules, seven mutations, each
failing exactly its test (`BeachTowersStandOnTheOceanfrontOnly`, `RoadWidthFollowsTheTownsStyle`,
`CrossroadsTownsHaveNoStreetTrees`, `CrossroadsLotsAreWooded` twice, `CrossroadsBuildingsAreSmall`).

**Measured** (same eye, 1600x900, DLSS quality): longs-noon 22-24 ms GPU (28-43 fps as the yard
trees came in), longs-golden 23, longs-night 24-26, longs-rain 24; the street view 20-24 ms. The yard
trees cost about 2 ms on the Longs views. The noon view was wrong twice over before it was right: at the planned eye and yaw, the frame was the player's own
camera at the spawn house, because the player controller had taken the view target back (`the view
target had become DTGPlayerController_0` in the log); photo mode now re-asserts it every frame.

**What the four pictures say, against a Longs photograph:** the road is two lanes with grass to its
edge and nothing but pines and live oaks over the yards; the buildings are single-storey on their
lots; the towers on the horizon are North Myrtle Beach's oceanfront, 560 m off at this scale; golden
hour has long shadows across the road; night has one lamp at the crossroads, a few lit windows per
house and a moonlit sky; rain falls out of a grey sky onto a wet road. What still says generated: the
houses are the same house (one facade module, shutters or not, one roof), the road has no centre line,
no shoulder and no ditch, the trees are scans scaled to size, the lawn is one green, there are no
driveways, fences, mailboxes or power lines on the lots, and the horizon beyond 900 m is bare.

**Not done from the brief:** real road layouts, land cover and elevation for Longs (OpenStreetMap,
NLCD and 3DEP are the licensed sources; nothing fetched yet); the architecture, road and terrain
work above; a 500 m slice measured at native resolution (the four views were shot with DLSS; the
earlier audit's native numbers stand for the street). No new assets, purchases or licences.


## Visual fidelity audit, 20 October

Kenny's brief: investigate before guessing, then the smallest high-impact set, validated on identical
views. The investigation used `Tools/Art/inventory_assets.py` (every asset with its Nanite flag),
`-DTGGpuProfile` read by `Tools/gpu_profile.py`, `-DTGMemReport`, and the logs. Before and after are the
tour's 17 fixed views at the same clock, 1600x900, DLSS quality; the before set is kept in
`Saved/Photos/before` with its log and memory report.

**What the inventory says.** 1,176 materials, 325 Nanite static meshes: the City Sample car kits, the
Poly Haven trees (pines and firs among them), the Megascans palms, grasses, kerbs, bins and poles are all
Nanite. The only big non-Nanite static meshes are City Sample's own full-detail `_LOD` car meshes (up to
3.2 M triangles), which nothing draws. Everything procedural (terrain chunks, roads, pavements, buildings,
the ocean) is `UProceduralMeshComponent`, which cannot be Nanite. The Megascans road surface
(`Fine_American_Road`) and the yellow road-line decal are imported and used by nothing.

**Root causes found and fixed.**
- `[VSM] Non-Nanite Marking Job Queue overflow`: one ocean sheet from four kilometres past the coast to
  four past the far side, a non-Nanite shadow caster covering every page of the shadow map at every
  level. The water casts no shadow now. The warning fires once at frame 2, while the world is first
  built, and never again (eleven times across the earlier logs).
- `Ray Tracing Geometry requested memory over budget (~406/400 MiB)`: the resident geometry pool is
  400 MB by default and the scene asked 402-408 (static BLASes 398 MB: the procedural world, which
  nothing streams out), so the engine evicted every frame. `r.RayTracing.ResidentGeometryMemoryPoolSizeInMB=768`:
  requested 408 MB, resident 444, no eviction, no warning; video memory in use 5.3 GB of 7.1 at the car
  view (was 5.7).
- Shadow depths were half the frame: 12.8 of 27.2 ms GPU on the street view, 9.7 of it non-Nanite
  virtual shadow maps, because every world mesh was Movable (the default for a component made in code)
  and the sun turned every frame with the clock, and a directional light that has moved at all drops
  its whole cached shadow map. Now the world's meshes and instance components are Static and the sun and
  moon turn in steps of a third of a degree (a 20 m shadow moves 12 cm a step). Nanite shadow rendering
  went 2.8 to 0.9 ms (cached); non-Nanite 9.7 to 7.4.
- Exposure was not physical: a 6 "lux" sun against 2,000-lumen street lamps and 14,000-lumen room lights,
  with a night held 2.5 stops under to paper over it. Now: a 100,000 lux sun with per-pixel atmosphere
  transmittance (the horizon reddens by physics, not a colour ramp), the weather's fraction of it under
  cloud, a 3 lux moon, 12,000-lumen lamps, the camera metering between EV 0 and EV 16 and half a stop
  under by day, and the windows' glow in nits through the Night parameter (`ADTGSky`, `DTGWorldBuilder`).

**Measured, before to after:** street 27.2 to 23.4 ms GPU (36 to 40 fps); downtown 22.6 to 20.8;
country-road 23.3 to 20.2; person 25.5 to 20.4; walker 27.9 to 20.7; kerb 27.6 to 21.3; night-street
34.9 to 22.6 (22 to 31 fps); night-downtown 32.0 to 20.8 (25 to 33 fps). Native 1600x900 with DLSS off:
street 22.0 ms, house 28.0, downtown 23.5 (39, 30, 34 fps). DLSS ray reconstruction costs 2.7 ms of
post-processing here and saves about 1 ms of rendering, because the frame is shadow-bound, not
pixel-bound. Without pedestrians and traffic (`-DTGPedestrians=0 -DTGTraffic=0`) the street is 19.8 ms
with non-Nanite shadows at 6.9: the crowd is not what remains. Held fifteen seconds before the profile (`-DTGSettle=15`) the street is 20.9 ms GPU (42 fps) with non-Nanite shadows at
6.2 and the house 23.3 with 7.7: it does not settle away. What remains is the procedural world itself being
redrawn into the shadow map every frame, and the cause is not found. The candidates, in order: a
`UProceduralMeshComponent` draws through the dynamic path, with no cached static mesh draw commands, so its
shadow pages may never be treated as static however its mobility is set; the whole town's buildings and
surfaces are one component each, so any section change recreates the proxy and drops every page; and chunk
streaming. The step that settles it either way is building chunks as real static meshes (Nanite where the
build allows), which also gets them out of the non-Nanite shadow path altogether.

**What the pictures say** (`Saved/Photos/before` against `Saved/Photos`, by eye): the day views are the
same scene with a deeper sky, warmer sunlit surfaces and a stop less bleaching; the dusk view
(`golden-street`) is the first that looks like the reference evening, a low warm sun through the
atmosphere, long shadows, haze towards the towers; the night views went from a grey rainy afternoon to a
dark street with lamp pools, warm windows and a moonlit sky, and stay readable.

**Remaining realism gaps, in the order they matter:** (1) the country outside town is bare bright lawn
with scattered trees to the horizon: there is no forest beyond the chunk radius and no pine stands, which
is the world generator, not the renderer; (2) downtown is glass office towers, and Conway has none; (3)
roads have no markings, wear or shoulders, and the imported Megascans road surface and line decal are
unused; (4) facades are flat textured boxes; (5) terrain is one lawn material; (6) the crowd's skinned
shadows (about 0.5 ms). Session briefs for 1 to 5 exist (golden-hour, conway-facades, roads-ground,
sc-setting) and wait on the worktree trust prompt.

**New dependencies, purchases, licences:** none. Everything used was already in the project: City
Sample and Megascans through Fab, Poly Haven CC0, NVIDIA DLSS under NVIDIA's licence.

**Not verified:** the Longs screenshot the brief refers to never arrived, so the views are the tour's
17 viewpoints around the spawn; night-time play (not just stills) under the new exposure; the half-stop
daytime bias in rain and fog; the shadowless ocean from the beach at low sun.

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

**Character art.** Done as far as the City Sample crowd goes (above). Still missing: a run, getting in
and out of a car, the conversation poses the NPC minds already drive, and the per-person skin and
clothing colours the pack keeps in per-instance data.

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
