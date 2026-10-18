# ADR-0001: A dependency-free physics core

- **Status:** accepted
- **Date:** 2026-10-18

## Context

Pool realism lives in the ball's *spin*: sliding friction and the transition to
rolling, collision-induced throw, spin transfer at a cushion, and squirt. A
general-purpose 2D rigid-body engine (Box2D, Chipmunk) resolves contacts between
2D bodies whose angular state is a single scalar about the screen normal. It has
no place to put the ball's angular velocity about the rolling axis, so it cannot
represent throw or the spin/velocity coupling at the cloth.

We also need the simulation to be reproducible: same shot in, same state out,
across machines and compilers.

## Decision

`pool_physics` is written from scratch with **no third-party dependency**: no SDL,
no physics engine, no math library, no RNG, no I/O, no globals. A ball is a rigid
sphere with a 2D position and velocity plus a full 3D angular velocity.

The library compiles with `-ffp-contract=off` so floating-point contraction
cannot fuse an `a*b + c` into an FMA and change the last bits of a result.
Contraction is not the only cross-toolchain variance, so the golden replay
compares a state digest quantised to a micron rather than exact bits; bit
equality is asserted only between runs of the same binary (see ADR-0003).

## Consequences

- Realism is controllable and inspectable: every coefficient is a named constant
  with a cited measured range (see ADR-0003).
- The simulation is testable headlessly. The whole physics suite runs in
  milliseconds with no display, assets or network, which is what makes the golden
  replay test practical in CI.
- Determinism is achievable because the dependency surface is small enough to
  reason about.
- We own the physics. The rail model in particular is an approximation with one
  documented deviation from measured data, and closing it needs a contact-time
  rubber model we would have to write ourselves.
- Anything the front-end needs beyond positions and velocities (screen
  transforms, textures, input) stays in `pool_app_lib`, never in the core.
