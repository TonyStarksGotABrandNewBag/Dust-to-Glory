# Roadmap

The first milestone was every feature, basic. That's where the project is now: all of it exists and works,
most of it is deeper than "basic", and what's thin is thin on purpose.

## Done

**The simulation core** — 79 tests, builds in seconds without an engine, CI on Linux, macOS and Windows.

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

## Next

In the order it's worth doing:

1. **Art.** Everything is engine primitives right now: buildings are boxes, cars are boxes with cylinders
   for wheels, people are boxes. The shapes, scales and positions are all correct, so this is swapping
   meshes and materials in, not rebuilding anything. Vehicle models and a vertex-colour terrain material
   would change the look of the whole game on their own.
2. **Interiors.** Buildings are solid shells with working doors and menus; walking inside them — shops,
   dealer showrooms, your own furnished home — is the biggest single gain in how the world feels.
3. **Deeper NPC conversation.** The minds are there and they remember; what they can say back is short.
4. **Animation.** Characters slide rather than walk; vehicles have no doors that open.
5. **More of the state.** Florida next, as a second state file — the generator doesn't care which.
6. **Mod support.** The data files are already the mod format; what's missing is loading them from a
   folder the player controls, and a way to ship a set of them.
7. **Save slots and a front end.** Saving works; choosing a slot from a menu doesn't exist yet.

## Thin on purpose

- **Buildings have no interiors yet.** Every one is interactable through its service menu, which is what
  makes "100% interactable" true today; walking in comes with the interiors work above.
- **Pedestrian and hero combat is minimal.** Powers push cars and people around; there's no fighting
  system behind it yet.
- **One state.** The map generator takes a state file; only South Carolina has one.
