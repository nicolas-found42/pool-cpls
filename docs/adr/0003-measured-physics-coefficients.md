# ADR-0003: Measured physics coefficients, banded tests

- **Status:** accepted
- **Date:** 2026-10-18

## Context

Physics constants attract invented numbers. A plausible-looking `0.01` will
survive review because nothing in the codebase can tell it from `0.014`. The
mistake is invisible until someone compares the game against a real table.

Independent measurements exist for the quantities that matter here: sliding
friction and rolling resistance (Mathavan, Jackson & Parkin, *Am. J. Phys.*
77(9), 2009), coefficients of restitution and spin deceleration (Dr. Dave's
constants page), and the equipment dimensions (WPA Recommended Equipment
Specifications). The WPA Rules of Play also specify the rules we implement.

## Decision

- Every coefficient in `MaterialParams` and `TableSpec` carries the **measured
  range** in a comment, not just a number, and cites a source that
  `docs/research/dev-toolchain-and-physics.md` lists with a URL.
- Tests assert the **band**, not the chosen value: "sliding friction is inside
  0.15-0.40", "ball-to-rail COR is inside 0.60-0.90", "spin deceleration is
  inside 5-15 rad/s²". A retune that leaves the measured range fails the suite.
- Where the model provably cannot match measurement, that is **stated as a
  deviation with the measured reference value**, in the constant's own comment,
  and asserted with the model's honest range. The rail's perpendicular rebound is
  the current example: measured ≈ 0.5, modelled ≈ 0.65.
- A change in physics is visible as a change in the golden fingerprint
  (`tests/replay/expected_fingerprint.txt`), which is re-pinned deliberately and
  reviewed as part of the change.

## Consequences

- Numbers in this repo are auditable: `grep` a coefficient and you find its
  source and its permitted range.
- "Is 0.72 right?" becomes answerable: it is the measured nominal, and the test
  says what would make it wrong.
- Some real behaviour stays explicitly unmodelled rather than faked. A known
  deviation with a number attached is more useful than a tuned constant that
  hides the gap.
- The tests encode physics expectations, so loosening one to make a change pass
  is a signal to investigate the model, not the test.
