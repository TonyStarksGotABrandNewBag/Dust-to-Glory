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

**Geometry and interiors** — buildings with storeys, windows, roofs, doors and steeples; cars built from
their body style and real dimensions; people; and rooms you can walk into, furnished with the furniture you
bought. All generated in code and drawn with vertex colours, so the game ships no art files.

**Florida** — a second playable state, chosen from the front end, with the Keys reached by causeway.

**Mods** — any data file can be replaced from a `Mods/` folder, including a whole new state.

## Next

In the order it's worth doing:

1. **Art.** The generated shapes are a floor, not a ceiling: they are the right size, in the right place,
   with the right silhouette, which is exactly what makes them straightforward to replace with modelled
   meshes and proper materials. Vehicle models would change the look of the game on their own.
2. **Animation.** Characters slide rather than walk, and vehicles have no doors that open.
3. **Deeper conversation.** The minds remember and wake up; what they can say back is still short.
4. **Combat.** Powers push cars and people around; there is no fighting system behind it yet.
5. **Weather and night.** The clock and the weather exist in the simulation but the sky doesn't change.
6. **More states.** The generator takes a state file and two exist; a third is a day of data entry.
7. **Multiplayer.** Nothing in the split prevents it — the simulation is deterministic — but nothing
   supports it yet either.

## Thin on purpose

- **Interiors are one room.** Walking in works everywhere and the fixtures suit the role, but a shop is
  not yet a shop with a back office.
- **No skeletal animation.** Everything moves rigidly, which is the single most noticeable gap.
- **Two states.** South Carolina and Florida.
