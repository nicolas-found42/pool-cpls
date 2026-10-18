#!/usr/bin/env python3
"""Single entry point for every development task in pool-cpls.

Why a Python script and not a Makefile: the same commands must work on macOS,
Linux and Windows, where make, the shell, and even the path separator differ.
Python 3.8+ is the only prerequisite and it is already on every CI runner.

Agents: run `python3 tools/dev.py doctor` first, then `check` before you claim
work is done. `check` is the same gate CI runs.

    doctor        report the toolchain and warn on version skew
    configure     configure a preset (default: dev)
    build         build a preset
    test          run ctest for a preset (accepts ctest args after --)
    check         format-check + tidy + cppcheck + build + test   <- the gate
    format        rewrite files with clang-format
    format-check  report formatting differences (no writes)
    tidy          run clang-tidy over the compile database
    cppcheck      run cppcheck over the compile database
    coverage      build + run + report line coverage
    fuzz          run the libFuzzer target for N seconds
    bench         run the micro-benchmark
    run           build and launch the game
    clean         delete the build directory
"""

from __future__ import annotations

import argparse
import glob
import re
import os
import shlex
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

# ---------------------------------------------------------------------------
# Pinned tool versions. Keep in sync with .pre-commit-config.yaml.
# ---------------------------------------------------------------------------
PINNED_CLANG_FORMAT_MAJOR = 23
PINNED_CLANG_TIDY_MAJOR = 23
MIN_CMAKE = (3, 28)

SOURCE_GLOBS = [
    "include/**/*.hpp",
    "src/**/*.cpp",
    "tests/**/*.cpp",
    "bench/**/*.cpp",
]

# Directories searched last, after $PATH, when a tool is not found. Homebrew's
# LLVM is keg-only, so clang-format/clang-tidy are not on PATH by default.
FALLBACK_PATHS = [
    # Apple's own LLVM tools: their profile format matches Apple clang, whereas
    # a Homebrew LLVM of a different major refuses to read Apple's .profraw.
    "/Library/Developer/CommandLineTools/usr/bin",
    "/opt/homebrew/opt/llvm/bin",
    "/usr/local/opt/llvm/bin",
    "/opt/homebrew/opt/llvm@23/bin",
    "/usr/lib/llvm-23/bin",
    "/usr/lib/llvm-22/bin",
]


class Failure(Exception):
    """A command failed; the message is already user-facing."""


def run(cmd: list[str], *, capture: bool = False, check: bool = True, env: dict | None = None):
    printable = " ".join(shlex.quote(part) for part in cmd)
    print(f"\033[1;34m$\033[0m {printable}", flush=True)
    result = subprocess.run(
        cmd,
        cwd=ROOT,
        text=True,
        capture_output=capture,
        env={**os.environ, **(env or {})},
    )
    if check and result.returncode != 0:
        if capture:
            sys.stderr.write(result.stdout or "")
            sys.stderr.write(result.stderr or "")
        raise Failure(f"command failed with exit code {result.returncode}: {printable}")
    return result


def which(name: str) -> str | None:
    found = shutil.which(name)
    if found:
        return found
    for directory in FALLBACK_PATHS:
        candidate = Path(directory) / name
        if candidate.is_file() and os.access(candidate, os.X_OK):
            return str(candidate)
    return None


def which_versioned(stem: str, minimum: int) -> tuple[str | None, int | None]:
    """Find the newest `stem-N` on PATH, then the plain name, then a fallback."""
    for version in range(minimum + 2, 10, -1):
        found = shutil.which(f"{stem}-{version}")
        if found:
            return found, version
    plain = which(stem)
    if plain:
        return plain, tool_major(plain)
    return None, None


def tool_major(binary: str) -> int | None:
    """Major version from `<tool> --version`, e.g. "clang-format version 23.1.3"."""
    try:
        out = subprocess.run([binary, "--version"], capture_output=True, text=True, timeout=30)
    except OSError:
        return None
    text = (out.stdout or "") + (out.stderr or "")
    for match in re.finditer(r"(\d+)(?:\.\d+)*", text):
        candidate = int(match.group(1))
        if 10 <= candidate <= 40:
            return candidate
    return None


def env_or_tool(env_name: str, stem: str, minimum: int) -> tuple[str | None, int | None]:
    explicit = os.environ.get(env_name)
    if explicit:
        return explicit, tool_major(explicit)
    return which_versioned(stem, minimum)


def clang_format() -> str:
    binary, _ = env_or_tool("CLANG_FORMAT", "clang-format", PINNED_CLANG_FORMAT_MAJOR)
    if not binary:
        raise Failure(
            "clang-format not found. Install LLVM 23 "
            "(`brew install llvm` on macOS, `apt install clang-format-23` on Ubuntu) "
            "or set CLANG_FORMAT=/path/to/clang-format."
        )
    return binary


def clang_tidy() -> str:
    binary, _ = env_or_tool("CLANG_TIDY", "clang-tidy", PINNED_CLANG_TIDY_MAJOR)
    if not binary:
        raise Failure(
            "clang-tidy not found. Install LLVM 23 (`brew install llvm`) or set "
            "CLANG_TIDY=/path/to/clang-tidy."
        )
    return binary


def source_files() -> list[str]:
    files: list[str] = []
    for pattern in SOURCE_GLOBS:
        files.extend(glob.glob(str(ROOT / pattern), recursive=True))
    return sorted(set(files))


def build_dir(preset: str) -> Path:
    return ROOT / "build" / preset


def cmake_configure(preset: str, extra: list[str]) -> None:
    run(["cmake", "--preset", preset, *extra])


def cmake_build(preset: str, targets: list[str], jobs: str | None) -> None:
    cmd = ["cmake", "--build", "--preset", preset]
    if jobs:
        cmd += ["--parallel", jobs]
    if targets:
        cmd += ["--target", *targets]
    run(cmd)


# ---------------------------------------------------------------------------
# Commands
# ---------------------------------------------------------------------------
def cmd_doctor(args: argparse.Namespace) -> int:
    print("pool-cpls toolchain\n")
    problems: list[str] = []
    notes: list[str] = []

    cmake = which("cmake")
    if cmake:
        version = run([cmake, "--version"], capture=True).stdout.splitlines()[0]
        print(f"  cmake        {version}")
        parts = version.split()[2].split(".")
        if tuple(int(p) for p in parts[:2]) < MIN_CMAKE:
            problems.append(f"cmake {'.'.join(parts[:2])} is older than {'.'.join(map(str, MIN_CMAKE))}")
    else:
        problems.append("cmake not found")

    cxx = which("c++") or which("g++")
    if cxx:
        line = run([cxx, "--version"], capture=True).stdout.splitlines()[0]
        print(f"  c++          {line}")
    else:
        problems.append("no C++ compiler found")

    for label, stem, env_name, expected in (
        ("clang-format", "clang-format", "CLANG_FORMAT", PINNED_CLANG_FORMAT_MAJOR),
        ("clang-tidy", "clang-tidy", "CLANG_TIDY", PINNED_CLANG_TIDY_MAJOR),
    ):
        binary, major = env_or_tool(env_name, stem, expected)
        if binary is None:
            notes.append(f"{label} not found: format/tidy will be skipped")
            print(f"  {label:<12} MISSING (expected major {expected})")
            continue
        print(f"  {label:<12} {binary} (major {major})")
        if major is not None and major != expected:
            problems.append(
                f"{label} major {major} does not match the pinned {expected}; "
                f"formatting or checks can differ from CI"
            )

    ninja = which("ninja")
    if ninja:
        print(f"  ninja        {ninja}")
    else:
        problems.append(
            "ninja not found: every CMakePresets.json preset uses the Ninja generator "
            "(`brew install ninja`, `apt install ninja-build`, or `choco install ninja`)"
        )

    cppcheck = which("cppcheck")
    if cppcheck:
        version = run([cppcheck, "--version"], capture=True).stdout.strip()
        print(f"  cppcheck     {version}")
    else:
        notes.append("cppcheck not found: cppcheck step will be skipped")

    pre_commit = which("pre-commit")
    print(f"  pre-commit   {pre_commit or 'MISSING (optional; hooks still run via dev.py)'}")

    print()
    for note in notes:
        print(f"  note: {note}")
    for problem in problems:
        print(f"  \033[1;31mproblem:\033[0m {problem}")
    if not problems:
        print("  \033[1;32mok\033[0m: toolchain matches the pinned versions")
    return 1 if problems else 0


def cmd_configure(args: argparse.Namespace) -> int:
    cmake_configure(args.preset, args.extra)
    return 0


def cmd_build(args: argparse.Namespace) -> int:
    cmake_build(args.preset, args.targets, args.jobs)
    return 0


def cmd_test(args: argparse.Namespace) -> int:
    cmd = ["ctest", "--preset", args.preset, "--output-on-failure", *args.extra]
    run(cmd)
    return 0


def cmd_format(args: argparse.Namespace) -> int:
    binary = clang_format()
    files = args.files or source_files()
    if not files:
        print("no source files found")
        return 0
    if args.check:
        run([binary, "--dry-run", "--Werror", *files])
        print(f"format: {len(files)} files clean")
    else:
        run([binary, "-i", *files])
        print(f"format: {len(files)} files rewritten")
    return 0


def cmd_tidy(args: argparse.Namespace) -> int:
    binary = clang_tidy()
    database = build_dir(args.preset) / "compile_commands.json"
    if not database.exists():
        raise Failure(
            f"{database} missing. Run `python3 tools/dev.py configure --preset {args.preset}` first."
        )
    files = args.files or [
        f for f in source_files() if f.endswith(".cpp") and "/tests/" not in f
    ]
    cmd = [binary, f"-p={database.parent}", "--quiet"]
    if args.fix:
        cmd.append("--fix")
    if args.strict:
        cmd.append("--warnings-as-errors=*")
    run([*cmd, *files])
    print(f"tidy: {len(files)} translation units checked")
    return 0


def cmd_cppcheck(args: argparse.Namespace) -> int:
    """Style/portability pass over first-party sources only.

    cppcheck reads the files directly rather than the compile database: the
    database contains FetchContent sources (Catch2, SDL3) whose style is not
    this repo's problem, and `--file-filter` has no negation in cppcheck 2.22.
    """
    binary = which("cppcheck")
    if not binary:
        print("cppcheck not installed; skipping (see docs/agents/tooling.md)")
        return 0

    # Translation units only. A header analysed on its own has no readers for
    # any of its members, which is how cppcheck invents `unusedStructMember`
    # reports for every struct in the public API. Headers are covered
    # transitively through the .cpp files that include them.
    files = [
        f
        for f in source_files()
        if f.endswith(".cpp") and "/_deps/" not in f and "/build/" not in f
    ]
    if not files:
        print("no source files found")
        return 0

    cache = build_dir(args.preset) / "cppcheck"
    cache.mkdir(parents=True, exist_ok=True)
    run([
        binary,
        f"--std=c++23",
        # No `style` group. cppcheck is not pinned (no Linux binary is published
        # for the current release), and its style findings move between versions
        # - CI's 2.13 reported useStlAlgorithm where 2.22 does not. Style and
        # semantic smells belong to clang-tidy, which is pinned; cppcheck keeps
        # the correctness-leaning groups.
        "--enable=warning,performance,portability",
        "--inline-suppr",
        "--suppress=missingIncludeSystem",
        "--suppress=unusedFunction",
        # passedByValue fires on Vec2/Vec3/Quat operator overloads. Those are
        # trivially copyable 16-byte aggregates: passing by value lets the
        # compiler keep them in registers, and C++17 guarantees the copy is
        # elided into the return value. Passing by const& here is a pessimisation.
        "--suppress=passedByValue",
        # Unit tests assert on the output of pure functions fed constant inputs;
        # cppcheck folds the arguments at analysis time and calls the assertion
        # always-true. That is the point of such a test, not a defect.
        "--suppress=knownConditionTrueFalse:tests/*",
        "--check-level=normal",
        "--error-exitcode=1",
        "--quiet",
        f"-I{ROOT / 'include'}",
        f"--cppcheck-build-dir={cache}",
        *files,
    ])
    print(f"cppcheck: {len(files)} files clean")
    return 0


def cmd_check(args: argparse.Namespace) -> int:
    """The gate: the same sequence CI runs, cheapest first."""
    steps = [
        ("format-check", lambda: cmd_format(argparse.Namespace(check=True, files=[]))),
        ("configure", lambda: cmd_configure(argparse.Namespace(preset=args.preset, extra=[]))),
        ("tidy", lambda: cmd_tidy(argparse.Namespace(
            preset=args.preset, files=[], fix=False, strict=True))),
        ("cppcheck", lambda: cmd_cppcheck(argparse.Namespace(preset=args.preset))),
        ("build", lambda: cmake_build(args.preset, [], args.jobs)),
        ("test", lambda: cmd_test(argparse.Namespace(preset=args.preset, extra=[]))),
    ]
    for name, step in steps:
        print(f"\n\033[1m==> {name}\033[0m")
        step()
    print("\n\033[1;32mcheck: all steps passed\033[0m")
    return 0


def cmd_coverage(args: argparse.Namespace) -> int:
    preset = "coverage"
    build = build_dir(preset)
    raw_profiles = sorted(build.glob("*.profraw"))

    cmake_configure(preset, [])
    cmake_build(preset, [], args.jobs)
    run(["ctest", "--preset", "coverage", "--output-on-failure"])

    raw_profiles = sorted(build.glob("*.profraw"))
    if not raw_profiles:
        raise Failure(
            f"no .profraw files in {build}. The coverage preset sets "
            "LLVM_PROFILE_FILE; check that the tests actually ran."
        )

    profile = build / "coverage.profdata"
    profdata = which("llvm-profdata")
    llvm_cov = which("llvm-cov")
    if profdata is None or llvm_cov is None:
        raise Failure(
            "llvm-profdata and llvm-cov are required for coverage. Install LLVM 23 "
            "(`brew install llvm`, `apt install llvm`) or set them on PATH. They must "
            "be the same major version as the compiler that produced the profiles."
        )

    run([profdata, "merge", "-sparse", *map(str, raw_profiles), "-o", str(profile)])
    binaries = sorted(str(binary) for binary in (build / "tests").glob("*_tests"))
    if not binaries:
        raise Failure(f"no test binaries under {build / 'tests'}")
    objects: list[str] = []
    for binary in binaries:
        objects += ["-object", binary]
    # Report on this repository's code only: the profiles also cover the
    # FetchContent sources (Catch2), and averaging those in makes the number
    # meaningless.
    run([
        llvm_cov,
        "report",
        binaries[0],
        f"-instr-profile={profile}",
        *objects,
        "-ignore-filename-regex=(tests|_deps|catch2|SDL)/",
    ])
    return 0


def cmd_fuzz(args: argparse.Namespace) -> int:
    preset = "fuzz"
    cmake_configure(preset, [])
    cmake_build(preset, ["pool_fuzz_strike"], args.jobs)
    target = ROOT / "build" / preset / "tests" / "pool_fuzz_strike"
    corpus = ROOT / "tests" / "fuzz" / "corpus"
    corpus.mkdir(parents=True, exist_ok=True)
    run([str(target), str(corpus), f"-max_total_time={args.seconds}"])
    return 0


def cmd_bench(args: argparse.Namespace) -> int:
    preset = args.preset
    cmake_configure(preset, ["-DPOOL_BUILD_BENCH=ON"])
    cmake_build(preset, ["pool_bench"], args.jobs)
    run([str(build_dir(preset) / "bench" / "pool_bench")])
    return 0


def cmd_run(args: argparse.Namespace) -> int:
    cmake_build(args.preset, ["pool_app"], args.jobs)
    run([str(build_dir(args.preset) / "pool_app")])
    return 0


def cmd_clean(args: argparse.Namespace) -> int:
    target = build_dir(args.preset)
    if target.exists():
        shutil.rmtree(target)
        print(f"removed {target}")
    else:
        print(f"{target} does not exist")
    return 0


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        prog="dev.py",
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument("--preset", default="dev", help="CMake preset (default: dev)")
    parser.add_argument("--jobs", default=None, help="parallel build jobs")
    sub = parser.add_subparsers(dest="command", required=True)

    # The same two options are accepted after the subcommand, so
    # `dev.py test --preset asan` works as well as `dev.py --preset asan test`.
    # SUPPRESS as the default keeps the subparser from clobbering a value the
    # top-level parser already bound.
    def add_common_options(subparser: argparse.ArgumentParser) -> None:
        subparser.add_argument(
            "--preset", default=argparse.SUPPRESS, help="CMake preset (default: dev)"
        )
        subparser.add_argument(
            "--jobs", default=argparse.SUPPRESS, help="parallel build jobs"
        )

    raw_add_parser = sub.add_parser

    def add_parser(name: str, **kwargs: object) -> argparse.ArgumentParser:
        created = raw_add_parser(name, **kwargs)  # type: ignore[arg-type]
        add_common_options(created)
        return created

    sub.add_parser = add_parser  # type: ignore[method-assign]

    sub.add_parser("doctor", help="report the toolchain")
    sub.add_parser("configure", help="configure a preset (CMake arguments after --)").add_argument(
        "extra", nargs="*", help="CMake arguments")
    build = sub.add_parser("build", help="build a preset")
    build.add_argument("targets", nargs="*", help="targets to build")
    test = sub.add_parser("test", help="run ctest (extra ctest arguments after --)")
    test.add_argument("extra", nargs="*", help="ctest arguments")

    fmt = sub.add_parser("format", help="rewrite files with clang-format")
    fmt.add_argument("--check", action="store_true", help="report differences instead of writing")
    fmt.add_argument("files", nargs="*", help="files to format (default: all sources)")

    tidy = sub.add_parser("tidy", help="run clang-tidy")
    tidy.add_argument("--fix", action="store_true")
    tidy.add_argument("--strict", action="store_true", help="treat warnings as errors")
    tidy.add_argument("files", nargs="*")

    sub.add_parser("cppcheck", help="run cppcheck")
    sub.add_parser("check", help="format-check, tidy, cppcheck, build, test")
    sub.add_parser("coverage", help="build, test and report coverage")
    fuzz = sub.add_parser("fuzz", help="run the fuzzer")
    fuzz.add_argument("--seconds", type=int, default=60)
    sub.add_parser("bench", help="run the benchmark")
    sub.add_parser("run", help="build and launch the game")
    sub.add_parser("clean", help="delete the build directory")

    # Arguments meant for CMake or ctest start with a dash, which argparse would
    # treat as an option of this script. They go after `--`, so the split is
    # explicit and their order and spelling survive verbatim:
    #   dev.py configure -- -DPOOL_SDL_HEADLESS=ON
    #   dev.py test -- -R physics --repeat until-fail:5
    argv_list = list(argv) if argv is not None else sys.argv[1:]
    trailing: list[str] = []
    if "--" in argv_list:
        split = argv_list.index("--")
        trailing = argv_list[split + 1 :]
        argv_list = argv_list[:split]

    args = parser.parse_args(argv_list)

    if trailing:
        pass_through = {
            "configure": "extra",
            "build": "targets",
            "test": "extra",
        }.get(args.command)
        if pass_through is None:
            parser.error(f"{args.command} takes no arguments after --")
        setattr(args, pass_through, [*getattr(args, pass_through), *trailing])

    handlers = {
        "doctor": cmd_doctor,
        "configure": cmd_configure,
        "build": cmd_build,
        "test": cmd_test,
        "format": cmd_format,
        "tidy": cmd_tidy,
        "cppcheck": cmd_cppcheck,
        "check": cmd_check,
        "coverage": cmd_coverage,
        "fuzz": cmd_fuzz,
        "bench": cmd_bench,
        "run": cmd_run,
        "clean": cmd_clean,
    }

    try:
        return handlers[args.command](args)
    except Failure as failure:
        print(f"\n\033[1;31merror:\033[0m {failure}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
