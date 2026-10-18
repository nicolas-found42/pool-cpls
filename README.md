# pool-cpls

A top-down 2D pool (billiards) game in C++23, built around a deterministic
physics core that models the ball as a rigid sphere with a full angular velocity:
sliding friction and the transition to rolling, collision-induced throw, spin
transfer at the cushions, draw and follow from the cue tip offset, and squirt.

Requires the tape measure less than a table does.

## Build and run

```sh
git clone https://github.com/nicolas-found42/pool-cpls
cd pool-cpls
python3 tools/dev.py doctor     # what is installed, what is missing
python3 tools/dev.py run        # configure, build and launch
```

Prerequisites: CMake 3.28+, a C++23 compiler, Ninja, and Python 3.8+.
SDL3 and Catch2 are found on the system if installed, otherwise fetched at a
pinned version with a verified hash. `doctor` names anything missing.

Controls: move the mouse to aim, `space` to shoot, `W`/`S` for power,
`Z`/`C` for side spin, `X`/`V` for draw and follow, `B` to recentre the tip,
`R` to re-rack, `Tab` to toggle the aim line, `Esc` to quit.

## Verify a change

```sh
python3 tools/dev.py check
```

That is the gate CI runs: format check, clang-tidy, cppcheck, build, tests. See
`AGENTS.md` for the full command list and `docs/agents/tooling.md` for which tool
owns which class of problem.

## What is where

| Path | What |
| --- | --- |
| `include/pool/physics/`, `src/physics/` | the simulation: no SDL, no I/O, no globals, no RNG |
| `include/pool/game/`, `src/game/` | eight-ball rules over a physics world |
| `include/pool/app/`, `src/app/` | SDL3 rendering and input |
| `tests/` | unit, invariant, golden-replay and fuzz targets |
| `docs/research/` | cited primary sources behind every physics constant |
| `docs/adr/` | decisions that are expensive to reverse |
| `docs/agents/` | agent-facing configuration and the tooling map |
| `GLOSSARY.md` | domain vocabulary |

## Physics is measured, not invented

Every coefficient carries its measured range in a comment and a test that asserts
the range, so a retune cannot silently leave the published band. Instruments:

- sliding friction and rolling resistance measured with high-speed imaging
  (Mathavan, Jackson & Parkin, *Am. J. Phys.* 77(9), 2009);
- coefficients of restitution and spin deceleration from Dr. Dave's pool physics
  constants;
- equipment dimensions from the WPA Recommended Equipment Specifications;
- rules from the WPA Rules of Play.

One deviation is known and documented rather than tuned away: a rigid
single-contact rail model under-predicts the speed loss of a forward-rolling ball
bouncing perpendicular off a cushion (about 0.65 of its inbound speed, against
roughly 0.50 measured). See `docs/adr/0003-measured-physics-coefficients.md`.

## Licence

Not yet chosen.
