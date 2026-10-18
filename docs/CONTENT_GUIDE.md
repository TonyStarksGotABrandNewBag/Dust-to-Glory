# Content guide

Everything in the game that isn't a rule is a file in `Content/Data`. They're plain JSON with `//`
comments allowed, loaded at startup, and staged next to the executable in a packaged build — so a player
can open them in a text editor and change the game.

`python3 Tests/check_content.py` parses all of them and fails if one is broken.

| File | Holds |
| --- | --- |
| `vehicles.json` | Brands, nameplates, generations and models: power, weight, gearing, tires, prices. |
| `parts.json` | Everything that can be fitted: engines, turbos, gearboxes, suspension, tires, bodywork. |
| `careers.json` | Jobs: requirements, levels, pay, workplace, whether a clean record is needed. |
| `powers.json` | Powers, their effects and the mutations each can roll. |
| `furniture.json` | Stores, catalogues, delivery. |
| `companies.json` | Public companies, their sectors and tickers, and the region's ministries. |
| `laws.json` | Laws that can be proposed, and what each changes. |
| `economy.json` | Prices, wages, the calendar year and the era's price level. |
| `state.json` | The playable state: outline, coast, towns, highways, rivers, lakes, mountains, venues. |
| `state-florida.json` | The same, for Florida. Any file starting with `state` shows up in the front end. |

## The one rule: nothing real

The places are real, because a map of South Carolina with invented town names would be pointless. Nothing
inside them is. Every make, model, business, ministry, school, team, person, phone number and email
address in this game is invented, and must stay that way.

- **Makes and models** are fictional throughout, and must not be a real one with a letter changed.
  Imitation is fine and intended — a brand that is clearly Detroit's answer to something, a nameplate with
  a real one's character — but the name, the slogan and the badge are ours.
- **Phone numbers** use `(843) 555-01xx`, which is reserved for fiction.
- **Email addresses** end in `.example`, which is reserved for the same reason.
- **Businesses, schools and teams** in real towns are invented. "Palmetto Coast University" is in
  Myrtle Beach; the real university there is not in this game.
- **No real people**, living or dead.

The content checker greps for the obvious real brand names. It can't catch everything, so the rule matters
more than the check.

## Adding a car

A model needs a brand and a nameplate to hang off. Brands carry a dealer group (sister brands share a
showroom) and whether they still sell new cars; nameplates carry their generations as `[start, end]` year
pairs, so a model knows which generation it belongs to.

```json
{
  "id": "harbinger-gt-1969",
  "brandId": "lancaster",
  "nameplate": "harbinger",
  "year": 1969,
  "body": "Coupe",
  "powerKw": 268,
  "massKg": 1570,
  "drive": "RWD",
  "engine": { "layout": "V8", "cylinders": 8, "displacementL": 6.4, "redlineRpm": 5800 },
  "gears": 4,
  "msrp": 3400
}
```

Prices are in 1990s money, which is the era the economy runs on. The catalog works out market value from
age, desirability and condition — a 1969 coupe is worth many times its sticker now, and the code that
decides that is in `Sim/Vehicles.cpp`.

## Mods

A mod is a folder under `Mods/` containing any of the files above. Whatever it holds replaces the shipped
version; everything else is untouched, several can be installed at once, and later folders win. A mod that
adds `state-ohio.json` adds a state to play in without touching anything else. The log says which mod each
file came from.

## Adding a town

A town is one entry in `state.json`:

```json
{ "name": "Loris", "lon": -78.890, "lat": 34.056, "style": "Rural", "grid": 2,
  "landmarks": [["Diner", "Highway 9 Diner"]] }
```

Real longitude and latitude; the generator divides distances between towns by `scale` (25) while keeping
streets and buildings full size. `style` picks what fills it (`Hub`, `City`, `Town`, `Rural`, `Beach`) and
`grid` is how many blocks a side. Exactly one town is `"start": true` and exactly one is the `Hub`, which
gets the motor mile and the main landmarks.

Towns that end up overlapping at game scale are pushed apart automatically, and any that would sit in the
sea are moved back onto land, so approximate coordinates are fine.

## Adding a highway

```json
{ "name": "SC-905", "route": ["Longs", "Conway"] }
```

A route is a list of town names and `[lon, lat]` waypoints in order. The generator brings the road to each
town's edge, detours around towns it doesn't serve, joins crossings into the road graph, and lifts the
deck onto a bridge wherever it meets water.
