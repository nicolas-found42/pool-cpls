# Glossary

The project's vocabulary. Use these terms in code, tests, issues and ADRs; the
physics literature uses them precisely and drifting to synonyms loses that.

## Table and equipment

| Term | Meaning here |
| --- | --- |
| **playing surface** | The rectangular area between cushion noses: 2.54 x 1.27 m for a 9-foot table |
| **cushion** / **rail** | The rubber nose bounding the playing surface. "Rail" is used in the code, "cushion" in the sources |
| **nose** | The front edge of the cushion that the ball strikes; its height is 63.5% of the ball diameter above the bed |
| **pocket mouth** | The opening between opposing cushion noses: 4.5 in at a corner, 5 in at a side pocket |
| **head / foot** | The two halves of the table; the head spot is a quarter of the length from the head rail, the foot spot three quarters |
| **cloth** | Nap-free worsted fabric covering the bed; the source of sliding and rolling friction |
| **rack** | The triangle of fifteen object balls, apex on the foot spot, eight ball below the apex |
| **break** | The first shot of a rack, cue ball in hand behind the head string |

## Ball motion

| Term | Meaning here |
| --- | --- |
| **rolling** | Motion with zero slip at the cloth contact point: `v = omega x (-R z)` |
| **sliding** | Motion with non-zero contact slip; friction acts on the slip direction |
| **slip** | Velocity of the ball's material point touching the cloth, `u = v + omega x r` |
| **english** | Vertical-axis spin (`omega_z`), i.e. side spin |
| **draw / follow** | Back spin / top spin about the horizontal axis perpendicular to the shot |
| **throw** | The small object-ball deflection caused by tangential friction at the ball/ball contact |
| **squirt** | Cue-ball deflection away from the side the tip strikes, before the ball leaves the tip |
| **swerve / massé** | A curved cue-ball path from an elevated cue with side spin; an approximation here |
| **check-up** | The pronounced speed loss when a forward-rolling ball rebounds from a cushion, because its spin fights the new direction |

## Simulation

| Term | Meaning here |
| --- | --- |
| **substep** | One fixed simulation increment (`SimulationParams::fixed_dt`, 1/480 s) |
| **world** | `pool::physics::World`: the ball set, table geometry and stepping |
| **contact** | A recorded collision event: ball/ball, cushion or pocket |
| **fingerprint** | A 64-bit hash of the dynamic state, used to detect unintended physics change |
| **golden** | The committed fingerprint of a fixed break (`tests/replay/expected_fingerprint.txt`) |
| **invariant** | A property that must hold for any input: energy never increases, momentum is conserved, balls never overlap |
| **hots** | A scratch: the cue ball is pocketed |

## Rules

| Term | Meaning here |
| --- | --- |
| **group** | Solids (1-7) or stripes (9-15), assigned on the first legal pot at an open table |
| **open table** | Groups not yet assigned; any ball except the eight may be struck first |
| **foul** | Cue ball pocketed, wrong ball first, or no rail after contact |
| **spotting** | Returning a ball to the foot spot line; used for the eight ball potted on the break |
| **illegal break** | A break that pockets nothing and drives fewer than four object balls to a rail |

## Tooling

| Term | Meaning here |
| --- | --- |
| **preset** | A named configure/build/test configuration in `CMakePresets.json` |
| **the gate** | `python3 tools/dev.py check`, the same sequence CI runs |
| **banded test** | A test asserting a coefficient lies inside its measured range, not equal to a chosen value |
