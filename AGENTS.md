## Agent skills

### Issue tracker

Issues and specs live as GitHub issues in `nicolas-found42/pool-cpls`. See `docs/agents/issue-tracker.md`.

### Triage labels

Namespaced vocabulary: one `status:*` state, one `type:*` type, one `priority:*`, optional `size:*`, plus modifiers. See `docs/agents/triage-labels.md`.

### Domain docs

Single-context: `GLOSSARY.md` and `docs/adr/` at the repo root. See `docs/agents/domain.md`.

---

# pool-cpls

A top-down 2D pool (billiards) game in C++23: a deterministic physics core, an
eight-ball rules engine, and an SDL3 front-end.

## Commands

`tools/dev.py` is the only entry point. Do not invoke cmake, clang-format,
clang-tidy or cppcheck directly: the script resolves tool versions, picks the
build directory and keeps the local and CI gates identical.

```sh
python3 tools/dev.py doctor        # toolchain + version-skew report; run this first
python3 tools/dev.py check         # THE GATE: format-check, tidy, cppcheck, build, test
python3 tools/dev.py build         # build the dev preset
python3 tools/dev.py test          # ctest
python3 tools/dev.py test -R physics -- -j8   # arguments pass through to ctest
python3 tools/dev.py format        # rewrite files with clang-format
python3 tools/dev.py tidy          # clang-tidy over the compile database
python3 tools/dev.py cppcheck      # static analysis
python3 tools/dev.py run           # build and launch the game
python3 tools/dev.py coverage      # build, test, report line coverage
python3 tools/dev.py fuzz --seconds 60
python3 tools/dev.py bench
python3 tools/dev.py clean
```

`check` is the contract: if it fails, the work is not done. It is the same
sequence CI runs, so a green `check` locally means a green CI.

Presets exist for the cases `check` does not cover: `debug`, `release`, `asan`
(ASan+UBSan), `ubsan`, `coverage`, `ci` (warnings are errors). Use
`--preset <name>`, e.g. `python3 tools/dev.py test --preset asan`.

## Layout

| Path | Owns |
| --- | --- |
| `include/pool/physics/`, `src/physics/` | simulation only: no SDL, no I/O, no globals, no RNG |
| `include/pool/game/`, `src/game/` | eight-ball rules over a physics world |
| `include/pool/app/`, `src/app/` | SDL3 rendering and input |
| `tests/physics/`, `tests/game/`, `tests/app/` | unit and invariant tests, mirroring the layers |
| `tests/replay/` | golden fingerprint of a fixed break; `tests/replay/expected_fingerprint.txt` |
| `tests/fuzz/` | libFuzzer target |
| `bench/` | dependency-free micro-benchmark |
| `cmake/` | warnings, sanitizers, dependency resolution |
| `tools/dev.py` | the task runner described above |
| `docs/agents/` | agent-facing config (issue tracker, triage labels, domain docs, tooling map) |
| `docs/research/` | cited primary-source notes behind the coefficients and the toolchain |
| `docs/adr/` | decisions that are expensive to reverse |
| `GLOSSARY.md` | domain vocabulary - use these terms in code, issues and tests |

## Which tool for which job

The full reasoning, including why each tool is trusted and what was deliberately
disabled, is in `docs/agents/tooling.md`. The short version:

- **Layout** is clang-format's job alone. Never argue style in a review; run
  `dev.py format`.
- **A type-aware smell** (suspicious conversion, dead store, missed `move`) is
  clang-tidy's. It needs `compile_commands.json`, so configure first.
- **Whole-program or portability issues** are cppcheck's.
- **Anything the compiler already reports** stays a compiler warning; the
  overlapping clang-tidy checks are disabled on purpose.
- **CI YAML** is actionlint's.
- **Physics behaviour** is pinned by tests, not by inspection. If you change a
  coefficient, a test that asserts the measured band should tell you why.

## Conventions

- **Physics is the only place that owns time.** The simulation never reads a
  wall clock; the app accumulates real time and calls `World::step()` a whole
  number of times per frame. Anything that makes the simulation depend on frame
  rate is a bug.
- **Keep the physics core dependency-free.** Anything from `include/pool/physics/`
  must build without SDL and without third-party includes.
- **Adding a coefficient?** Add it to `MaterialParams` with the measured range in
  a comment, and a test that asserts the range. Values in this repo cite sources
  in `docs/research/dev-toolchain-and-physics.md`.
- **Changing physics changes the golden.** Re-pin it deliberately
  (`POOL_UPDATE_GOLDEN=1 dev.py test -R replay`) and review the diff; a changed
  fingerprint means the physics changed.
- **Never loosen a test to make a change pass.** If a physical invariant fails,
  the model is wrong or the test encodes a real expectation - find out which.
- Formatting, includes and naming are enforced: run `dev.py format` before you
  claim work is done.
