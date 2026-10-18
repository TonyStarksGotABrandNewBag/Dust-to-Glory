# Audio

The engine note isn't a recording. Each cylinder firing launches a pressure pulse into its bank's exhaust
header; the pulses ring through a pipe resonance and a muffler filter, and what comes out depends on the
engine's real crank layout and the parts fitted.

That's why a cross-plane V8 burbles and a flat-plane one screams: the cross-plane fires unevenly within
each bank, so the pulses never settle into a regular beat, while the flat-plane fires each bank evenly.
Nothing about that is special-cased — it falls out of `FiringSchedule` in `Sim/EngineAudio.cpp`.

## What's layered in

| Layer | Driven by |
| --- | --- |
| Exhaust pulses | Firing order, rpm, load, header length per bank |
| Pipe resonance | Exhaust part fitted (stock, sport, straight pipe) |
| Intake | Throttle and load |
| Turbo whistle and blow-off | Boost, and lifting off while boosted |
| Supercharger whine | Belt speed, so it tracks rpm exactly |
| Gear whine | Straight-cut gears, if fitted |
| Overrun pops | Trailing throttle, more with an aggressive cam or anti-lag |
| Electric motor | A rising whine instead of combustion |
| Tires and road | Slip, speed and the surface under the wheels |
| Wind | Speed |

`RoadSynth` handles the last two: squeal when the tires let go, a rolling roar that grows with speed, and
wind over the top.

## Weather

`WeatherSynth` does the same job for rain and thunder. Rain is filtered noise in two bands -- a high hiss
off everything at once and a lower patter -- mixed by how hard it is falling, so a downpour is more roar
than hiss rather than the same loop turned up. A roof between you and it, a car or a building, puts the
sound through a low-pass instead of swapping to a different file.

Thunder is a noise burst that opens with a crack and falls away into a low roll, and it is **delayed by
how far off the strike was**: about three seconds to the kilometre, which is the count everybody knows. A
flash the player sees across the marsh is heard eight or nine seconds later, by which time the sky may
have flashed again, so several claps can be in the air at once.

The sky decides when lightning strikes, because it is the sky that lights up; `ADTGWeatherFx` watches the
flash and asks for the clap. A brighter flash is a nearer strike, which is the only cue the player has.

## How it runs in the engine

`UDTGEngineAudio` is a `USynthComponent`. `OnGenerateAudio` runs on the audio render thread and renders
both synthesisers straight into the mixer's buffer — no allocation, no locks. The game thread hands over
rpm, throttle, load, boost, speed and slip through `SynthCommand`, which is Unreal's own way of getting a
value to that thread safely.

Because the sound is generated from what the engine is doing, it tracks rpm continuously. There's no
crossfade between loops, no sample pitching, and a rev limiter bouncing sounds like a rev limiter
bouncing.

## Recorded samples later

The synthesiser is the floor, not the ceiling. Recorded rpm loops can be blended per engine family: keep
the synthesised layer for transients (limiter, shifts, pops, blow-off) and crossfade recordings underneath
for the steady-state body of the note. The hook for it is `SoundProfile` in `Sim/Tuning.h` — add a sample
set id there and mix it in `EngineSynth::Render`.

## Mixing

`UDTGEngineAudio` exposes `EngineLevel` and `RoadLevel`. Everything is mono at the source and positioned
in the world by the component's attenuation, so a car passing you is a car passing you.
