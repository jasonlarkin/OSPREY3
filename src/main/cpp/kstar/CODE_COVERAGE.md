# Code Coverage for the C++ K* Port

This doc is written to implement/maintain **coverage reporting** while porting OSPREY tests.

Companion doc (how to *generate new inputs/tests* that buy coverage):

- `COVERAGE_GUIDED_TESTING.md`

## Goals

- **Prove code paths were exercised** (line/branch/function coverage).
- **Keep coverage builds separate** from performance builds (no `-O3 -march=native` in coverage mode).
- **Integrate with CTest/GTest** so `ctest` drives coverage runs.

## Coverage Options (C++ Tooling)

### GCC + gcov + lcov/genhtml (HTML)

- **Best when**: building with GCC (common on Linux/WSL).
- **Produces**: HTML report (line + branch coverage depending on flags).
- **Tools**:
  - `gcov`: generates coverage data from `--coverage` instrumentation
  - `lcov`: captures/merges `gcov` data into `.info`
  - `genhtml`: renders HTML from `.info`

### Clang + llvm-profdata + llvm-cov (HTML/text)

- **Best when**: building with Clang; generally the nicest reporting UX.
- **Produces**: HTML or text reports with strong path/branch info.
- **Tools**:
  - `llvm-profdata merge`
  - `llvm-cov show` / `llvm-cov report`

### Clang SanitizerCoverage (edge/PC coverage; fuzzing-oriented)

- **Best when**: you want “did this edge execute?” style coverage (often paired with fuzzers).
- **Not** the same as line coverage; useful for safety/reliability work later.

## gcov/lcov vs llvm-cov

- **Instrumentation format**
  - **gcov/lcov/gcovr**: emits/reads GCC-style `*.gcno` (compile-time) + `*.gcda` (runtime).
  - **llvm-cov/llvm-profdata**: emits/reads LLVM `*.profraw` (runtime) merged into `*.profdata` plus Clang coverage mapping in the binaries.

- **Branch/edge model**
  - **gcov** “branch coverage” is largely **CFG-edge** oriented; denominators tend to be larger and branch % often lower.
  - **llvm-cov** “branch coverage” comes from Clang’s coverage mapping; it also reports **region coverage** (no direct lcov equivalent).

- **Headers/templates**
  - Header-only and template-heavy code often shows **larger deltas** between tools because each tool attributes inline code differently.
  - Treat header coverage as directional unless you standardize on one backend.

- **Fuzzing ergonomics**
  - **llvm-cov** pairs naturally with Clang + libFuzzer + sanitizers (one toolchain).
  - **gcov/lcov** pairs naturally with GCC-driven unit/integration test runs; fuzzing is still possible, just less “native”.

Policy for this repo:

- Use **lcov prod-only A/B diffs** to quantify “did fuzz corpus replay increase *production* coverage?” (stable in this repo).
- Use **llvm-cov HTML** for deep local inspection (regions, inline attribution), and do not expect branch totals to match lcov.

## Current Repo Support (kstar CMake)

The kstar CMake supports a GCC/Clang coverage mode and can generate multiple “mixtures” of reports from the same run:

- **CMake option**: `KSTAR_ENABLE_COVERAGE=ON`
- **Coverage flags** (GNU/Clang):
  - compile: `-O0 -g --coverage`
  - link: `--coverage`
- **Custom targets**:
  - **`kstar_coverage`**: runs tests and generates *all* reports (same as `kstar_coverage_all`)
  - **`kstar_coverage_all`**: production + tests
  - **`kstar_coverage_prod`**: production-only (filters out `src/test/**` from the report)
  - **`kstar_coverage_tests`**: tests-only
- **Report outputs** (HTML):
  - all: `${build_dir}/coverage/kstar-all/index.html`
  - prod: `${build_dir}/coverage/kstar-prod/index.html`
  - tests: `${build_dir}/coverage/kstar-tests/index.html`

Notes:
- Coverage capture uses `lcov --base-directory` so it works correctly under WSL paths like `/mnt/c/...` (prevents “no valid records”).

See: `src/main/cpp/kstar/CMakeLists.txt` (search `KSTAR_ENABLE_COVERAGE` and `kstar_coverage`).

## How to Generate Coverage (GCC + lcov) in This Repo

### 1) Install tools (WSL/Linux)

```bash
sudo apt-get update
sudo apt-get install -y lcov
```

### 2) Configure a dedicated coverage build directory

Keep a separate build dir so normal dev builds stay fast and optimized.

```bash
cd $REPO_ROOT

cmake -S src/main/cpp/kstar -B build/cpp/kstar-coverage \
  -DBUILD_TESTING=ON \
  -DKSTAR_ENABLE_COVERAGE=ON \
  -DKSTAR_ENABLE_NATIVE_OPT=OFF
```

### 3) Build and run the coverage target

```bash
cmake --build build/cpp/kstar-coverage -j
cmake --build build/cpp/kstar-coverage --target kstar_coverage
```

### Optional: gcovr reports (Cobertura XML + HTML)

If you want a single-file coverage artifact for ingestion by other tools, run:

```bash
cmake --build build/cpp/kstar-coverage --target kstar_coverage_gcovr
```

Outputs:

- Cobertura XML (prod-only filter): `${repo}/build/cpp/kstar-coverage/coverage-gcovr/coverage.prod.cobertura.xml`
- HTML: `${repo}/build/cpp/kstar-coverage/coverage-gcovr/index.html`

### Optional: A/B compare prod-only coverage (baseline vs fuzz corpus replay)

If you are using libFuzzer and the corpus replay tests (label `fuzz_corpus`), you can generate two prod-only reports:

- baseline (excludes label `fuzz_corpus`)
- with fuzz corpus replay (includes label `fuzz_corpus`)

Run:

```bash
cmake --build build/cpp/kstar-coverage --target kstar_coverage_prod_compare_fuzz
```

Outputs:

- baseline: `${repo}/build/cpp/kstar-coverage/coverage/kstar-prod-baseline/index.html`
- with fuzz: `${repo}/build/cpp/kstar-coverage/coverage/kstar-prod-fuzz/index.html`

This currently includes (when built):

- `kstar.energy_matrix_loader_corpus_runner`
- `kstar.conf_search_astar_corpus_runner`

To see *which files* changed between baseline and fuzz, run:

```bash
./scripts/kstar_lcov_diff_prod.sh
```

To generate a machine-readable report under the build tree:

```bash
./scripts/kstar_coverage_prod_delta_report.sh
```

Interpreting the diff output:

- **`d_lines_hit`**: how many additional lines were hit in the fuzz run vs baseline
- **`d_funcs_hit`**: how many additional functions were hit
- **`d_branches_hit`**: how many additional branches were taken
- Most files will have 0 deltas; the value is in quickly pinpointing exactly *where* fuzzing is buying coverage.

Notes on interpretation:

- **`d_funcs_hit` can be 0 while fuzzing is still valuable**: it may be exercising *new branches/lines inside functions that were already entered* by baseline tests.
- A small delta (single-digit lines, tens of branches) is expected once baseline coverage is already high; interpret it as “remaining gaps are narrow and input-specific”.

The delta script also writes “hotspot” lists (baseline=0, fuzz>0):

- **Newly covered branches**: `${repo}/build/cpp/kstar-coverage/coverage/prod_coverage_delta.new_branches.tsv`
- **Newly covered lines**: `${repo}/build/cpp/kstar-coverage/coverage/prod_coverage_delta.new_lines.tsv`
- **Newly covered functions**: `${repo}/build/cpp/kstar-coverage/coverage/prod_coverage_delta.new_functions.tsv`

For quick review, it also writes a single markdown summary:

- `${repo}/build/cpp/kstar-coverage/coverage/prod_coverage_delta.summary.md`

### Promoting fuzz inputs into stable regressions (recommended)

Fuzzing produces two kinds of useful inputs:

- **Crash artifacts** (from `fuzz-artifacts/...`) → should be minimized and promoted as bug regressions
- **Non-crashing corpus inputs** (from `fuzz-corpus/...`) → can be promoted as “coverage regressions” (keep executing new paths even after fuzzer stops running)

Repo support:

- **Optional local inputs directory**: `build/cpp/kstar/test_data/fuzz/energy_matrix_loader/`
- **Regression CTest**: `kstar.energy_matrix_loader_regression`
  - labels: `coverage_regression`, `energy_matrix_loader`
  - always runs under `ctest` (always runs embedded seeds; replays local `*.bin` files if present)

Convenience scripts:

```bash
# promote any single file (artifact or corpus input) into repo-local regressions
./scripts/kstar_promote_fuzz_artifact_energy_matrix_loader.sh \
  build/cpp/kstar-fuzz/fuzz-corpus/energy_matrix_loader/<file>

# run just the regression test (must point ctest at a build tree)
ctest --test-dir build/cpp/kstar-coverage -R kstar.energy_matrix_loader_regression -V
```

### 4) Open the report

- Coverage HTML entry points:
  - all: `${repo}/build/cpp/kstar-coverage/coverage/kstar-all/index.html`
  - prod: `${repo}/build/cpp/kstar-coverage/coverage/kstar-prod/index.html`
  - tests: `${repo}/build/cpp/kstar-coverage/coverage/kstar-tests/index.html`

## Notes / Common Failure Modes

### lcov/genhtml not installed

If `lcov` or `genhtml` is missing, CMake prints:

- `Coverage requested (KSTAR_ENABLE_COVERAGE=ON) but lcov/genhtml not found`

and `kstar_coverage` will fail with a clear message.

### gcovr not installed

If `gcovr` is missing, `kstar_coverage_gcovr` will fail with an install hint:

```bash
pip install gcovr
```

or:

```bash
sudo apt-get install -y gcovr
```

### “Coverage is 0%”

Common causes:
- The tests didn’t run (use `ctest --output-on-failure` and confirm it executes).
- You generated coverage in a different build directory than the one you instrumented.
- You’re filtering out too much with `lcov --remove`.

### genhtml: “no valid records found”

This usually happens when `lcov` filtered out all records (common in WSL if `--no-external` is used without a base dir).
In this repo, `kstar_coverage` uses:

- `lcov --base-directory "${REPO_ROOT}"`

so the captured paths under `/mnt/c/...` map correctly back to the repo source tree.

### gcov warnings about timestamps / “source file is newer than notes file (.gcno)”

You may see messages like:

- `libgcov profiling error: ... overwriting an existing profile data with a different timestamp`
- `<source>.cpp: source file is newer than notes file '<...>.gcno'`

What they mean:

- The build tree contains stale coverage metadata/data from a previous compile/run.

Fix:

- Prefer running coverage via `kstar_coverage` (it resets counters).
- If warnings persist, do a clean rebuild of the coverage build directory:
  - `rm -rf build/cpp/kstar-coverage`
  - re-run CMake configure/build and `kstar_coverage`

### “My report includes GoogleTest / system headers”

The current `kstar_coverage` target filters out:
- `/usr/*`
- `${build_dir}/_deps/*` (FetchContent deps such as googletest)
- `*googletest*`, `*gtest*`

Adjust filtering in `CMakeLists.txt` if more exclusions are needed.

## Clang/llvm-cov Support

This repo supports a parallel coverage mode using Clang source-based coverage:

- **CMake option**: `KSTAR_ENABLE_LLVM_COVERAGE=ON` (requires Clang)
- **Tools**: `llvm-profdata`, `llvm-cov`
- **Outputs (HTML)**:
  - all: `${build_dir}/coverage-llvm/kstar-all/index.html`
  - prod-only: `${build_dir}/coverage-llvm/kstar-prod/index.html`

Typical flow (what the CMake target does):

```bash
LLVM_PROFILE_FILE="default_%p.profraw" ctest --output-on-failure
llvm-profdata merge -sparse default_*.profraw -o coverage.profdata
llvm-cov show ./path/to/test_binary -instr-profile=coverage.profdata -format=html -output-dir=coverage_html
```

Key docs:
- LLVM Source-based coverage: `https://clang.llvm.org/docs/SourceBasedCodeCoverage.html`
- llvm-cov usage: `https://llvm.org/docs/CommandGuide/llvm-cov.html`

### How to run llvm-cov coverage in this repo

Configure a dedicated build directory:

```bash
cmake -S src/main/cpp/kstar -B build/cpp/kstar-llvm-coverage \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DBUILD_TESTING=ON \
  -DKSTAR_ENABLE_LLVM_COVERAGE=ON \
  -DKSTAR_ENABLE_NATIVE_OPT=OFF
```

Build and run:

```bash
cmake --build build/cpp/kstar-llvm-coverage -j
cmake --build build/cpp/kstar-llvm-coverage --target kstar_llvm_coverage
```

## References: C++ Porting + Testing Docs in This Repo

Use these as “context packets” for another Cursor thread:

- **Test status tracker**: `src/main/cpp/kstar/TEST_PORTING_STATUS.md`
- **Detailed porting notes**: `src/main/cpp/kstar/TEST_PORTING_NOTES.md`
- **Master list of OSPREY tests to port**: `src/main/cpp/kstar/OSPREY_TESTS_TO_PORT.md`
- **Testing strategy**: `src/main/cpp/kstar/TESTING_STRATEGY.md`
- **Coverage-guided test generation (fuzzing / symbolic execution)**: `src/main/cpp/kstar/COVERAGE_GUIDED_TESTING.md`
- **Phase 1 notes**: `src/main/cpp/kstar/PHASE1_MINIMAL_KSTAR.md`


