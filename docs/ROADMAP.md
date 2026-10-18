# Roadmap

The first milestone was every feature, basic. That's where the project is now: all of it exists and works,
most of it is deeper than "basic", and what's thin is thin on purpose.

## Done

**The simulation core** — 81 tests, builds in seconds without an engine, CI on Linux, macOS and Windows.

- Driving: Pacejka tires, 500 Hz drivetrain, 240 Hz chassis with suspension, aero, collisions, launch
  control, traction control and ABS; manual or automatic.
- Cars: 92 fictional makes with generations back to the fifties; new, certified pre-owned, used, classic,
  restorations, resto-mods and a locator; parts, tuning and damage.
- The state: 21 real South Carolina towns at 1:25, 13 highways, rivers, lakes, mountains, the Atlantic,
  bridges and four event venues.
- People: NPC minds that remember, wake up and choose their own path; cars parked at houses you can make
  an offer on, by knocking, by note, by text or by email.
- A living: careers, shifts, education, businesses, equity, stocks, reputation, IP and royalties, hacking.
- Belief and power: ministries with tithes and integrity; elections from council to president; laws that
  change the rules.
- Trouble: police heat, courts, pleas, trials, co-defendants, jail, parole, appeals, expungement; five
  ways to break out and life as a fugitive; the engineered-outbreak scheme and the investigation that
  unpicks it.
- Game theory throughout: Nash bargaining, Rubinstein offers, prisoner's dilemma, inspection, Cournot,
  Bertrand.

**The engine layer** — the world builds itself at runtime, cars drive on the simulation's physics, the
engine note is synthesised per sample, input and HUD are built in code, traffic and pedestrians fill the
streets, and events run at the venues.

**Geometry and interiors** — buildings with storeys, windows, roofs, doors and steeples; cars built from
their body style and real dimensions; people; and rooms you can walk into, furnished with the furniture you
bought. All generated in code and drawn with vertex colours, so the game ships no art files.

**Florida** — a second playable state, chosen from the front end, with the Keys reached by causeway.

**Mods** — any data file can be replaced from a `Mods/` folder, including a whole new state.

**Day, night and weather** — the sun follows the game clock, the weather the simulation already generated
now darkens the road, closes in the fog and takes grip away from the tires, and headlights come on at dusk.

**Fights** — health for the player and whoever they are up against, fists, powers and cars all hitting
differently, assault charges for hitting a bystander, bounties for putting a villain down, and a hospital
bill rather than a game over.

**Walking** — people are jointed and swing their legs and arms at the pace they are actually travelling.

**Conversation** — a passer-by is from somewhere, carries a problem, knows something and knows people.
You can ask about the town, their work, what they have been seeing and what is going around; ask twice and
they answer shorter, a third time and they tell you they already did, and keep at it and they walk off.
Somebody who trusts you tells you what is wrong in their life and you can put it right with money, which
buys more than money usually does. What they know has a price, settled by splitting the gain from telling
against the risk of telling, so a friend hands over what a stranger will not sell. And whatever you do in
front of them, they repeat: word travels through the people they actually know, arriving attributed and
losing a little conviction at every mouth, until the town has an opinion of you that nobody gave it.

**Rain** — a storm you can see and hear. Three layers of streaks around the camera, built as geometry like
everything else, leaning into the wind and into your own speed, so driving fast lays the rain over towards
you and it radiates from a point ahead. Lightning lights the sky and the fog, and the thunder that follows
it is held back by how long the sound would really take to arrive -- a far strike is a roll several seconds
after the flash. The rain and the thunder are synthesised sample by sample, like the engine note, so a
downpour gets louder and lower rather than crossfading, and from inside a car you hear the roof.

**The buildings work.** Every service a building's sign offers now reaches the system behind it: the
job board lists what you qualify for, the showroom lists its own brands, the lot haggles, the stock
desk trades, the legislature takes a lobbying cheque, the ministry can be founded. And the everyday
ones -- a meal, a hotel room, an hour in the gym, a doctor, the window at the police station -- are a
simulation of their own, so what they cost and what they give you is one set of numbers rather than
two.

**North Carolina**, and roads that are built rather than draped. The third state climbs from sea level to
2,037 m, which the generator had no answer for: a highway was pinned to the terrain at each of its own
vertices and followed the hillside between them, so US-64 came out of the Blue Ridge at 56% and I-26 hit
244% where US-64 crossed over it -- the two roads were being averaged into one hillside instead of one
passing over the other. Highways are now cut and filled to their own graded profile, crossings belong to
whichever road is underneath, and every deck is tapered so it cannot rise or fall faster than 6%. All
three states now come out at or under that, bridge ramps and mountain passes included; before this work
South Carolina reached 8.5% and nobody had noticed.

## Next

In the order it's worth doing:

1. **Art.** The generated shapes are a floor, not a ceiling: they are the right size, in the right place,
   with the right silhouette, which is exactly what makes them straightforward to replace with modelled
   meshes and proper materials. Vehicle models would change the look of the game on their own.
2. **Multiplayer.** Nothing in the split prevents it — the simulation is deterministic — but nothing
   supports it yet either.

## Thin on purpose

- **Interiors are one room.** Walking in works everywhere and the fixtures suit the role, but a shop is
  not yet a shop with a back office.
- **Animation is procedural.** Limbs swing at the right cadence; there is no skeletal animation, because
  there are no animation assets to play.
- **Three states.** South Carolina, Florida and North Carolina. A fourth is a day with an atlas.
- **The engine layer has never been compiled.** It was written somewhere without Unreal installed. It
  has been read closely six times and that found real things -- a player spawning at the world origin, a
  camera inside the car, pedestrians that could not move, a car that could not be driven because its own
  driver was blocking it, four compile errors -- but reading is not building. Expect to fix a few things
  the first time it goes through a compiler. `Tests/check_headers.py` catches the one class a script can.
