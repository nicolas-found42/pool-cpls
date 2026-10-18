# ADR-0002: CMake presets plus one task runner

- **Status:** accepted
- **Date:** 2026-10-18

## Context

A C++ project on three platforms accumulates commands: configure, build, test,
format, tidy, cppcheck, coverage, fuzz, run. If those commands live in a README,
they rot; if they live in CI YAML, developers cannot run them; if they live in a
Makefile, they do not work on Windows without a shim.

Agents make this sharper: a contributor who cannot see the *one* command that
validates a change will guess, and will guess differently from CI.

## Decision

- **`CMakePresets.json` owns configuration.** Every preset is generator- and
  OS-neutral, keeps its build directory under `build/${presetName}`, and exports
  `compile_commands.json`. No absolute paths, so the same file works locally and
  on every runner.
- **`tools/dev.py` owns invocation.** One Python 3 script (no third-party
  imports) exposes `doctor`, `configure`, `build`, `test`, `check`, `format`,
  `format-check`, `tidy`, `cppcheck`, `coverage`, `fuzz`, `bench`, `run`,
  `clean`. It resolves tool binaries, honours `CLANG_FORMAT`/`CLANG_TIDY`
  overrides, and prints every command it runs.
- **`check` is the gate**, and it is the same sequence CI runs.

## Consequences

- A developer, an agent and CI execute identical steps; "works locally, fails in
  CI" is reduced to genuine environment differences.
- Python is the only prerequisite, and it is preinstalled on all three CI images.
- `dev.py` is the place to add a tool, which keeps local and CI from drifting
  apart: the CI job calls the script, it does not reimplement it.
- The script is a thin wrapper by design. Project knowledge goes into presets and
  CMake options, not into Python.
