# Dev toolchain and pool physics: research notes

Primary-source findings that the build, test, lint and physics layers of this
repo are built on. Every claim below is attributed; claims measured **in this
repo** are marked as such and are reproducible with the commands in
`docs/agents/tooling.md`.

Written 2026-10-18. Versions were checked on that date.

## 1. Build system and dependencies

**CMake floor.** CMake 4.0 *removed* compatibility with versions older than 3.5,
and CMake 3.31 deprecated anything below 3.10, so a `cmake_minimum_required`
below 3.5 is now a hard error rather than a warning. This repo requires 3.28:
well clear of both, and new enough for `FILE_SET`, `CXX_MODULES` and preset
schema v6. ([cmake_minimum_required](https://cmake.org/cmake/help/latest/command/cmake_minimum_required.html))

**Presets carry no absolute paths.** `CMakePresets.json` is written with
`binaryDir: ${sourceDir}/build/${presetName}` and no generator- or OS-specific
paths, so the same preset configures on a developer Mac and on every CI runner.
Presets are the only entry point: `tools/dev.py` shells out to `cmake --preset`
rather than repeating flags.

**Dependency resolution order.** `find_package(X CONFIG QUIET)` first, pinned
release tarball with a verified SHA-256 second. An installed package is 1-2
seconds faster and lets a distro's SDL3 be used; the tarball path keeps CI and a
fresh clone working with no package manager. A tarball also avoids the git-clone
cost of `FetchContent`'s default `GIT_REPOSITORY` mode, and hashing it is the
only honest way to pin a non-git transport.

**Floating-point contraction must be switched off.** GCC and Clang both enable
FP contraction by default, which lets the compiler fuse `a*b + c` into an FMA and
change the last bits of a result. This repo compiles the physics library with
`-ffp-contract=off` so a golden replay is comparable across compilers. Verified
in this repo: with contraction on, the golden fingerprint is not portable.

## 2. Testing

**Catch2 v3.16.1** is the current release. The CMake integration exports
`Catch2::Catch2` and `Catch2::Catch2WithMain`; use the latter unless a custom
`main` is needed. ([Catch2 CMake integration](https://raw.githubusercontent.com/catchorg/Catch2/devel/docs/cmake-integration.md),
[releases](https://github.com/catchorg/Catch2/releases))

**Per-test CTest registration costs one line of setup that is easy to get
wrong.** `catch_discover_tests` lives in Catch2's `extras/` directory, and the
upstream doc states plainly that with `FetchContent` the `include(Catch)` call
*fails* unless `CMAKE_MODULE_PATH` is extended with
`${catch2_SOURCE_DIR}/extras`. This repo hit exactly that failure while being
built; the fix is in `cmake/Dependencies.cmake`, and `tests/CMakeLists.txt`
falls back to one CTest entry per binary when the extras are absent (an
installed Catch2 need not ship them).

**Apple Silicon + the Xcode generator needs `DISCOVERY_MODE PRE_TEST`.** The
default `POST_BUILD` discovery runs the test binary to list tests, and macOS on
Apple Silicon refuses to execute an unsigned binary that Xcode signs only after
the post-build step. Same limitation as CMake's `gtest_discover_tests`. This repo
uses Ninja, so the default is fine, but the caveat is recorded for anyone
generating Xcode projects.

**Test layers and what each catches.** Chosen so no layer duplicates another:

| Layer | Catches | Example in this repo |
| --- | --- | --- |
| Unit | arithmetic, sign conventions, thresholds | rolling condition, slip velocity, impulse sign |
| Invariant | physics that must hold for any input | energy never increases, momentum conserved, no penetration, bounds respected |
| Golden replay | unintended behaviour change | a fixed break must produce one fingerprint |
| Smoke | link/runtime/rendering breakage without a display | `pool_app --smoke` under SDL's dummy driver |

**Determinism is a property of the build, not of the code alone.** No RNG, no
wall-clock reads, no unordered iteration, and no FP contraction give a
reproducible simulation. Verified in this repo: two runs of the same break
produce identical fingerprints, and the suite tolerates no floating-point
fuzziness in the golden value - it compares exact bits.

## 3. Lint, format and static analysis

**Pin the formatter.** clang-format output is not stable across majors: the same
file formatted by two majors can differ, which turns "format check" into a
version negotiation. This repo pins major 23 in two places that must agree -
`tools/dev.py::PINNED_CLANG_FORMAT_MAJOR` and the
`pre-commit/mirrors-clang-format` `rev` - and `tools/dev.py doctor` reports a
mismatch as a problem rather than letting CI fail later. Verified in this repo:
LLVM 23.1.3 formats the tree cleanly and idempotently.

**Division of labour.** Overlapping linters produce duplicate findings and
erode trust in the gate, so each tool owns one job:

| Tool | Owns | Does not do |
| --- | --- | --- |
| compiler (`-Wall -Wextra -Wconversion …`) | type errors, conversions, shadowing | whole-program dataflow |
| clang-format | layout only | semantics |
| clang-tidy | type-aware smells (`bugprone-*`, `clang-analyzer-*`, `performance-*`, curated `modernize-*`/`readability-*`) | layout |
| cppcheck | cheap whole-program checks, uninitialised data, portability | type-aware analysis |
| actionlint | the GitHub Actions workflows | C++ |

**Finding quality beats finding count.** Deliberately disabled, with reasons:
`bugprone-easily-swappable-parameters` and `bugprone-narrowing-conversions`
duplicate compiler warnings that are already errors; `cppcoreguidelines-*` and
`modernize-use-trailing-return-type` are style opinions that fight the numeric
code. Two cppcheck checks needed the same treatment, and both are recorded next
to their suppression in `tools/dev.py`:

- `passedByValue` on `Vec2`/`Vec3`/`Quat` operator overloads is wrong for
  trivially copyable 16-byte aggregates.
- `knownConditionTrueFalse` fires on unit tests that assert a pure function's
  output for constant input, which is the entire point of such a test.
- `unusedStructMember` appeared only because headers were being analysed as
  standalone translation units; analysing `.cpp` files instead removed the whole
  class of false positive rather than suppressing it.

**Strictness belongs in the gate, not in the editor.** `-Werror` and
`--warnings-as-errors` are off by default locally and on in CI, so a newer
compiler on a developer's machine cannot brick their build.

## 4. Rendering and the app layer

Facts read from the SDL3 3.4.18 headers
([SDL_render.h at release-3.4.18](https://github.com/libsdl-org/SDL/blob/release-3.4.18/include/SDL3/SDL_render.h)),
confirmed locally against the installed Homebrew headers:

- `SDL_CreateRenderer(SDL_Window*, const char* name)`; passing `NULL` for the
  name asks SDL to pick a suitable driver, which is the portable choice.
- **SDL3 has no circle or ellipse primitive.** `SDL_RenderGeometry` is the
  drawing primitive to build one from, and it takes
  `SDL_Vertex { SDL_FPoint position; SDL_FColor color; SDL_FPoint tex_coord; }`
  - `SDL_FColor`, not `SDL_Color`, so a colour must be converted to floats.
- **Without an index buffer, `SDL_RenderGeometry` consumes the vertex array as a
  triangle *list*, not a fan.** Verified in this repo: a fan emitted as
  `center, p0..pn` renders as broken arcs. Every circle here emits whole
  triangles instead.
- `SDL_RenderDebugText` exists and renders a fixed 8x8 cell per character, which
  is why the HUD and the ball numbers are centred against
  `SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE`.

## 5. Pool physics

**Equipment constants**, from the WPA Recommended Equipment Specifications
([PDF](https://wpapool.com/wp-content/uploads/2024/01/RECOMMENDED-EQUIPMENT-SPECIFICATIONS.pdf)):

| Quantity | Specification | In this repo |
| --- | --- | --- |
| Playing surface (9 ft) | 100 x 50 in = 2.54 x 1.27 m | `TableSpec::length/width` |
| Ball diameter | 2.25 in = 0.05715 m | `kBallRadius` = 0.028575 m |
| Ball mass | 5.5-6 oz = 156-170 g | `kBallMass` = 0.170 kg |
| Cushion nose height | 63.5% of ball diameter above the bed | rail contact offset, §5 below |
| Corner pocket mouth | 4.5-4.625 in | `corner_mouth` = 0.1143 m |
| Side pocket mouth | 5-5.125 in (0.5 in wider) | `side_mouth` = 0.127 m |
| Cloth | nap-free worsted, 80-85% wool | justifies a worsted friction model |

**Friction and restitution coefficients**, from measured sources rather than
folklore. Dr. Dave's constants page
([physical properties](https://drdavepoolinfo.com/faq/physics/physical-properties/))
gives ball-cloth sliding friction 0.15-0.4 (typical 0.2), rolling resistance
0.005-0.015, ball-ball COR 0.92-0.98, ball-rail COR 0.6-0.9, and ball-cloth spin
deceleration 5-15 rad/s². Mathavan, Jackson & Parkin measured the same
quantities independently on a high-speed camera: sliding 1.75-2.40 m/s²
(mu_s ≈ 0.18-0.25) and pure rolling 0.124-0.126 m/s² (mu_r ≈ 0.0128), noting
Marlow's 0.011-0.024 for American pool
([AJP 77(9) 788](https://pubs.aip.org/aapt/ajp/article/77/9/788/235539/Application-of-high-speed-imaging-to-determine-the)).
Every default in `MaterialParams` sits inside those bands, and tests in
`tests/physics/` assert the bands rather than the chosen numbers, so a future
retune cannot silently leave the measured range.

**The sliding-to-rolling transition has a closed form.** Contact slip decays at
`(7/2) mu_s g`, so the impulse that cancels a slip speed `s` is `2 m s / 7`, and
a ball that slides without spin ends up rolling at `5/7` of its initial speed.
The implementation uses the impulse form (not a force over a timestep) because
it cancels the slip exactly instead of overshooting into reverse friction.

**Rolling resistance is a Coulomb force, not viscous drag.** Verified in this
repo: modelling it as a velocity-proportional decay left balls creeping forever
and the table never settled - the test suite caught it as "a rolling ball comes
to rest" timing out at 200 000 substeps. With a constant deceleration
`a = mu_r g`, a ball rolled at 1 m/s travels 2.55 m in the model against
`v^2 / (2 mu_r g)` = 2.55 m analytically, and a break now settles in about 8 s of
simulated time.

**The rail is not a mid-line contact.** The cushion nose sits at 63.5% of the
ball *diameter* above the bed - 1.27 R above the cloth - while the ball's centre
sits at R, so the nose strikes about 0.27 R above centre. That offset is not
cosmetic: the normal impulse acquires a torque about the rail-parallel axis,
which is the axis of the ball's rolling spin, so the rail cancels part of the
ball's forward spin on the way out. Measured **in this repo**: with the offset
modelled, a forward-rolling ball driven perpendicular into a rail leaves at 0.72
of its inbound speed instead of 0.47 (the check-up that a rail-agnostic model
produces).

**A rigid rail cannot match both measured rail numbers at once.** Measured data
reports a normal-component rail COR of 0.6-0.9 *and* a perpendicular rebound of
a forward-rolling ball at roughly 0.5 of its inbound speed, because the rail
kills part of the spin. Sweeping the model's two rail parameters
(**in this repo**) shows those two targets cannot both be met by a single rigid
contact: reaching 0.50 rebounds forces the COR to the bottom of the band, which
then under-predicts the stun-shot bounce. The repo therefore keeps
`cushion_restitution = 0.72` (the measured nominal), asserts the stun-shot band,
and records the rolling case as a known deviation with the measured reference
value. Closing it needs a contact-time rail model with rubber hysteresis - that
is a follow-up, not a fudge factor.

**Engine choice.** A general-purpose 2D rigid-body engine is the wrong tool for
this: pool realism lives in the *spin* state - sliding friction, throw, cushion
spin transfer, squirt - and a 2D engine's contact solver has no representation
for the ball's angular velocity about the table normal or the rolling axis. That
is why `pool_physics` has no third-party dependency and models the ball as a
rigid sphere with a full 3D angular velocity in a 2D plane of motion.

## 6. Where rules came from

The eight-ball engine cites the WPA Rules of Play
([PDF](https://wpapool.com/wp-content/uploads/2026/01/2026.01.02-WPA-Rules.pdf)):

- **4.3(d)** "If no object-ball is pocketed, at least four object-balls must be
  driven to one or more rails, or the shot results in an illegal break." The
  break requirement is implemented as written, scaled down when the table holds
  fewer than four object balls so the rule stays satisfiable.
- **4.3(e)** "Pocketing the 8-ball on a legal break shot is not a foul" - the 8
  is spotted (1.5) and play continues. This repo originally scored that as a
  loss; the research corrected it, and a test now pins the spotting behaviour.
- **4.2** Rack: apex on the foot spot, the 8 directly below the apex, one ball
  from each group on the lower corners.
- **4.4** Open table: any object ball except the 8 may be struck first; a legal
  pot assigns the group.

## Sources

- https://cmake.org/cmake/help/latest/command/cmake_minimum_required.html
- https://raw.githubusercontent.com/catchorg/Catch2/devel/docs/cmake-integration.md
- https://github.com/catchorg/Catch2/releases
- https://github.com/libsdl-org/SDL/blob/release-3.4.18/include/SDL3/SDL_render.h
- https://wpapool.com/wp-content/uploads/2024/01/RECOMMENDED-EQUIPMENT-SPECIFICATIONS.pdf
- https://wpapool.com/wp-content/uploads/2026/01/2026.01.02-WPA-Rules.pdf
- https://drdavepoolinfo.com/faq/physics/physical-properties/
- https://pubs.aip.org/aapt/ajp/article/77/9/788/235539/Application-of-high-speed-imaging-to-determine-the
