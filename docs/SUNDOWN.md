# Cheats, and Sundown

## Cheats

There is no console and no debug menu. Cheats are dialled on the player's phone, which is already
how they talk to the world.

Every code is a word, and **the number is the word on a phone keypad** — `PAYDAY` is `1-999-729329`.
That mapping is computed in `DialFor()`, not written down twice, so the word and the number cannot
drift apart when a cheat is renamed. Either form works, and spaces, dashes and case are ignored.

Entering one always sends a text back. There is no cheat that changes the world silently.

**`bEverUsed` is saved and never cleared.** Turning a cheat back off does not clear it. This is not a
punishment — it is the player's game — but a save where a quarter of a million dollars was dialled in
should not be indistinguishable from one where it was earned.

| Code | What it does |
|---|---|
| `PAYDAY` | $250,000, which in a 1990s economy buys a street |
| `BLUECHIP` | tomorrow's market moves, today |
| `GROUNDFLOOR` | any business sells at book value |
| `LANDSLIDE` | straight to the next office on the ladder |
| `KEYSTOTHECITY` | every make and model, on every lot |
| `SHOWROOM` | the current car perfect again |
| `TITANIUM` | the driven car stops taking damage |
| `STICKYTIRES` | far more grip than any tyre has |
| `BARNFIND` | a classic turns up, rough and complete |
| `COLDCASE` | wanted level and open charges gone |
| `MOSTWANTED` | five stars, immediately |
| `TIMESERVED` | out of jail; the conviction stays |
| `CLEANSLATE` | the whole record sealed |
| `ALLTHREE` | all three power slots filled |
| `FULLCHARGE` | instability cleared |
| `REROLL` | re-rolls the mutations |
| `HIGHNOON` | holds the clock where it stands |
| `FOURSEASONS` | rolls the weather on |
| `FEATHERFALL` | gravity at a fifth |
| `HALFSPEED` | everything at half pace |
| `GHOSTTOWN` | no traffic, nobody on the pavements |
| `RUSHHOUR` | three times the traffic |
| `SUNDOWN` | see below |

Some are acted on in the simulation (money, wanted level, jail, powers, the weather). The rest —
physics, traffic density, holding the clock, what a showroom will sell you — are read by the engine
layer through `GameState::CheatOn()`. The simulation only has to remember which are on.

## Sundown

### The one rule

The Pale cannot bear direct sunlight. Everything else follows from that.

Day is not safe time handed to the player as a gift — it is a budget. The sun is up for a fixed
number of minutes and every one of them is spent deciding how far from shelter you are willing to be
when it goes down. Dusk is the clock running out.

That rule is also why the buildings matter. This world already had interiors everywhere; in Sundown
every dark one is somewhere the Pale wait out the day. A warehouse at noon is more dangerous than a
street at dusk. That inverts the usual instinct to get inside, and turns a feature that was scenery
into the central risk.

### What this is not

There is no gore in `Sim/Sundown.cpp` and no mechanic that rewards it. Nothing tracks wounds or
damage to a body. The pressure comes from the light failing, from distance, from sound carrying, and
from knowing something has been counting which streets you keep going back to. A `Dread` value rises
in the dark and near them, and the engine turns it into sound and shadow. That is the entire horror
budget, deliberately.

### The light

`SunAt(Minute)` is one continuous arc from first light to last — `sin` of the day angle. `SafeSun`
(0.55) is the single threshold every question about light is measured against:

- `IsDaylight()` — is open ground safe
- `Exposure()` — how exposed you are right now
- `MinutesOfLightLeft()` — safe light remaining, or 0
- `MinutesUntilSafe()` — the wait until it returns, or 0

They share the threshold *and* the curve, so the number on the player's watch cannot contradict what
the world is doing. The first version had two of them measured differently: at seven in the evening
the countdown said the light had run out nine minutes ago while the rules still treated it as full
daylight. Worse, `SunAt` was not even continuous — the twilight ramps reached 1.0 while the midday
branch bottomed out at 0.50, so the sun jumped at the boundary and spent the afternoon dipping below
the safety threshold. Rendering a dusk is what showed it up, and `TestSundown` now checks both: every
minute of the day for agreement between the clocks and the danger, and every minute for continuity.

### The Alpha

Stalkers chase. Shriekers call others. There is exactly one Alpha, and it keeps count: forage the
same block three days running and it will be waiting the fourth. That is the inspection game from
`Sim/GameTheory.h` — the player picks where to forage, the Alpha picks where to watch — and it is the
difference between an obstacle and an opponent.

Its expectation approaches certainty without reaching it (capped at 0.95), because a player who knows
they will definitely be ambushed has no gamble left to take. It also forgets, slowly, if you stop
going there.

### The loop

Wake → decide how far to range → scavenge (which strips a place, so tomorrow is worth less there
than somewhere new) → be inside and boarded before dusk → survive the night → repeat.

Light is the real defence: a generator with fuel in it beats any amount of boarding, and the tests
isolate that specifically. Boards buy time; light stops them coming.

The long game is the cure — `Research()` needs both medical supplies and hours, neither substituting
for the other — and `Deploy()` hands it to the existing `OutbreakSystem`.

### Two ways in

1. **Dial `SUNDOWN`.** It starts from the next minute, in any save, and dialling it again puts the
   world back.
2. **Earn it.** `Begin()` takes the live `OutbreakSystem`. If an outbreak is already running — which
   it will be if the player engineered one through the conspiracy system and it got away from them —
   the population that turned is the population that was infected. The pandemic feature stops being
   a money scheme and acquires an ending.

### Still to do

The engine side: spawning the Pale, their movement and pack behaviour, the audio that carries the
dread, and the lighting work that makes a dark interior genuinely frightening rather than merely
dim. The economy is not yet rewired for barter — cash is inert in Sundown but the shops have not been
closed. All of that is phase two, and none of it can be verified here.
