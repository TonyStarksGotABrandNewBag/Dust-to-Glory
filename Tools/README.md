# Tools — how this project gets verified without an engine

The engine layer has never been compiled by Unreal, let alone run. Everything that is known to be
true about this world is known because one of these programs measured it or drew it.

They are deliberately terse — single-file, no headers, dense formatting — and they are not held to
the standard the rest of the codebase is. They are instruments, not product. What matters about them
is that they read the **real simulation**: there is no second implementation of a tree or a road or a
sunset anywhere in here. If one of these draws it, that is what the game draws.

## Building and running

Every one of them compiles the same way, from the repository root:

```sh
g++ -O2 -std=c++20 -I Source/DustToGlory Tools/<name>.cpp Source/DustToGlory/Sim/*.cpp -o /tmp/<name>
/tmp/<name> [arguments]
```

They read `Content/Data/` relative to the working directory. Set `DTG_DATA` to point elsewhere.

Images come out as binary PPM. To look at one:

```sh
python3 -c "from PIL import Image; Image.open('out.ppm').save('out.png')"
```

## What each one is for

| Tool | Answers | Found |
|---|---|---|
| `profile_load.cpp` | How long until the player can move? Parses content, generates a state, times a terrain chunk and a save/load round trip. | Established the 24–46 ms startup baseline, and that startup was never the problem — streaming was. |
| `profile_hot.cpp` | What do the per-frame terrain queries cost, per sample? Takes `indexed` or `exact` to compare the lookup grid against the exhaustive walk. | Showed `RoadHeightAt` at 2.97 µs/sample walking all 141 road segments; drove the spatial index that cut it to 0.69 µs. The checksum is printed so both modes can be shown to agree. |
| `profile_world.cpp` | What do vegetation and road paint cost per streamed chunk, and how many stripes does a state hold? | Caught road markings generating 144,274 stripes and 20 ms of load for North Carolina. Now 29,498 and 4.9 ms. |
| `render_drive.cpp` | What does a road actually look like from a driver's eye? Terrain, roads, markings and vegetation, depth-buffered, from inside a lane. | Confirmed the species mix reads as the Carolinas vs Florida. Also exposed that the paint was invisible — which turned out to be this renderer's affine depth interpolation, not the game. |
| `render_dusk.cpp` | What does Sundown's light curve look like as the day runs out? Same scene, lit entirely by `SundownState::SunAt`. | Found two real bugs: the player's countdown and the actual danger measured against different thresholds, and `SunAt` being discontinuous — the sun jumped at the twilight boundary and dipped below the safety threshold all afternoon. |
| `render_statemap.cpp` | Does the state look like the state? Top-down map of coastline, rivers, lakes, towns and roads. | Used to verify geography, town placement and road connectivity across all three maps. |

## The point

Three of the six found a real bug the test suite did not. That is not an argument against the tests —
it is an argument that *looking at the thing* catches a different class of error than asserting about
it. A test can only check what you thought to check. A picture shows you what you did not.

When adding a system that has a shape — terrain, weather, light, anything placed in the world — draw
it before believing it.
