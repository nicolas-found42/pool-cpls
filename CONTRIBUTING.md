# Contributing

## Before you start

```sh
python3 tools/dev.py doctor
```

`doctor` reports the toolchain against the versions this repo pins and treats a
mismatch as a problem, because a different clang-format major reformats the whole
tree.

## The loop

```sh
python3 tools/dev.py check     # format-check, tidy, cppcheck, build, test
```

Run `check` before you claim anything is done. It is the same sequence CI runs,
so green locally means green in CI. If `format-check` fails, run
`python3 tools/dev.py format`.

Useful narrower loops while working:

```sh
python3 tools/dev.py build
python3 tools/dev.py test                              # everything
python3 tools/dev.py test -- -R "rolling condition"    # one test case
python3 tools/dev.py test --preset asan                # the same tests under ASan + UBSan
```

Arguments meant for CMake or ctest follow `--`, so their spelling and order
survive: `python3 tools/dev.py configure -- -DPOOL_SDL_HEADLESS=ON`.

## Linux build dependencies

macOS and Windows need nothing beyond the compiler and CMake. On Linux, SDL3's
configure step requires the X11 or Wayland development libraries - its own
message points at
[SDL's Linux build dependencies](https://wiki.libsdl.org/SDL3/README-linux).
Install them to build the windowed app.

CI does not: it builds a fetched SDL3 with `-DPOOL_SDL_HEADLESS=ON`, which is
SDL's documented `SDL_UNIX_CONSOLE_BUILD` path, because a runner only needs the
dummy video driver for the headless smoke test.

## Where code goes

| Change | Lands in |
| --- | --- |
| simulation behaviour | `include/pool/physics/`, `src/physics/` |
| rules | `include/pool/game/`, `src/game/` |
| rendering, input, windowing | `include/pool/app/`, `src/app/` |
| a new check or command | `tools/dev.py` and the matching CI job |

The physics core stays free of SDL and third-party includes. If a change needs
one, it belongs in the layer above.

## Changing physics

Physics changes are the ones that bite later, so they have extra rules:

1. **Cite the number.** A new or changed coefficient carries its measured range
   in a comment, and the source goes into
   `docs/research/dev-toolchain-and-physics.md`. `docs/adr/0003` explains why.
2. **Assert the band, not the value.** Tests should fail if a coefficient leaves
   the measured range, not if someone adjusts it inside the range.
3. **Re-pin the golden deliberately.** A physics change moves the fingerprint:

   ```sh
   POOL_UPDATE_GOLDEN=1 python3 tools/dev.py test -- -R replay
   git diff tests/replay/expected_fingerprint.txt
   ```

   Review that diff as part of the change. An unexplained fingerprint change is a
   bug or an unreviewed behaviour change.
4. **Never loosen a test to make a change pass.** If an invariant such as
   "energy never increases" or "balls never overlap" fails, either the model is
   wrong or the test encodes a real expectation. Find out which, and write down
   the answer.

## Style

- clang-format owns layout. Do not hand-format; run `dev.py format`.
- clang-tidy and cppcheck opinions are configured in `.clang-tidy` and
  `tools/dev.py`. Suppressions name the check and the reason. See
  `docs/agents/tooling.md`.
- Use the words in `GLOSSARY.md` in identifiers, tests and commit messages.

## Commits

Conventional prefixes (`feat:`, `fix:`, `docs:`, `test:`, `build:`, `refactor:`,
`chore:`), imperative mood, one logical change per commit. If a commit moves the
golden fingerprint, say so in the message and say why.

## Issues and triage

Issues live in GitHub Issues. Labels are namespaced - one `status:*`, one
`type:*`, one `priority:*`, optional `size:*`, plus modifiers - and are described
in `docs/agents/triage-labels.md`.
