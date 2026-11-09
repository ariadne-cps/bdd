# Ariadne BDD

[![macOS Status](https://github.com/ariadne-cps/bdd/actions/workflows/macos.yml/badge.svg)](https://github.com/ariadne-cps/bdd/actions/workflows/macos.yml)
[![Ubuntu Status](https://github.com/ariadne-cps/bdd/actions/workflows/ubuntu.yml/badge.svg)](https://github.com/ariadne-cps/bdd/actions/workflows/ubuntu.yml)
[![Windows](https://github.com/ariadne-cps/bdd/actions/workflows/win.yml/badge.svg)](https://github.com/ariadne-cps/bdd/actions/workflows/win.yml)
[![Sanitizers](https://github.com/ariadne-cps/bdd/actions/workflows/sanitizers.yml/badge.svg)](https://github.com/ariadne-cps/bdd/actions/workflows/sanitizers.yml)
[![Coverage](https://github.com/ariadne-cps/bdd/actions/workflows/coverage.yml/badge.svg)](https://github.com/ariadne-cps/bdd/actions/workflows/coverage.yml)
[![codecov](https://codecov.io/gh/ariadne-cps/bdd/branch/main/graph/badge.svg)](https://codecov.io/gh/ariadne-cps/bdd)

Ariadne BDD is the standalone C++20 binary decision diagram layer for Ariadne. The repository is a container for decision-diagram implementations; the current implementation is an ROBDD with canonical nodes, ITE-based Boolean operations, evaluation and a thread-safe baseline implementation.
Although independently maintained and usable, this repository is also part of the [Ariadne repository structure](https://github.com/ariadne-cps/ariadne/blob/main/doc/structure.md).

The project is intentionally independent of Ariadne Kernel, Foundation, Numeric, Interval and Algebra. It depends directly on Ariadne Build and Ariadne Threading; Utility, Macro and Logging arrive through the Threading dependency chain.

## Current features

- Reduced ordered binary decision diagrams with variables ordered by increasing index.
- Canonical unique table.
- ITE computed table.
- Constants, variables, negation, conjunction, disjunction and exclusive-or.
- Evaluation under a total binary assignment.
- Thread-safe manager operations using a coarse-grained mutex baseline.
- A concurrent canonical-construction regression test.
- Python bindings for the current core BDD API.
- C++ and Python tutorials exercised on Unix and Windows CI.
- Release builds and installed-library tutorials on Unix and Windows; Debug C++/Python tests, AddressSanitizer/UBSan, ThreadSanitizer and coverage CI.

The current unique table and computed table are deliberately simple ordered maps protected by one mutex. They are a correctness baseline, not the intended final concurrent implementation.

## Dependencies

The repository contains these direct Git submodules:

- `ss/cfg` -> [ariadne-cps/build](https://github.com/ariadne-cps/build)
- `ss/thr` -> [ariadne-cps/threading](https://github.com/ariadne-cps/threading)
- `ss/pyt` -> [ariadne-cps/python-common](https://github.com/ariadne-cps/python-common), used only to build the Python bindings

Clone recursively so that all transitive dependencies and pybind11 are initialized.

## Build

```bash
git clone --recurse-submodules https://github.com/ariadne-cps/bdd.git
cd bdd
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel
ctest --output-on-failure
```

A C++20 compiler and CMake are required. Python bindings are built by default when a compatible Python development environment is available; set `-DBUILD_PYTHON_BINDINGS=OFF` to disable them.

## Python bindings

The standalone build produces the `pyariadne` module when Python 3.11 or newer and the Python development headers are available. The current binding exposes `RobddManager`, `Robdd`, `BddVariable` and `BinaryWord`.

Python tests use pytest. Configure with `-DPYTHON_ENFORCE_TESTS=ON` when Python and pytest must be present.

## Tutorials

A standalone C++ tutorial is available directly in `tutorial/`. It is designed to be built against an installed Ariadne BDD rather than directly against the repository source tree.

After installing Ariadne BDD to a prefix:

```bash
cmake -S tutorial -B tutorial-build -DCMAKE_PREFIX_PATH=/path/to/install
cmake --build tutorial-build
./tutorial-build/tutorial_bdd
```

A matching Python tutorial is available in `tutorial/tutorial_bdd.py`, alongside the C++ example. It reproduces the same sequence of examples using the installed Python bindings and can be run with `python tutorial/tutorial_bdd.py`.

The Unix and Windows Release CI jobs build and execute both tutorials against the installed library; the installed Python extension is checked to come from the installation prefix. Debug CI runs C++ and Python regression tests.

## ThreadSanitizer

```bash
mkdir build-tsan
cd build-tsan
cmake .. -DCMAKE_BUILD_TYPE=Debug -DTHREAD_SANITIZER=ON
cmake --build . --parallel
ctest --output-on-failure
```


## Manual parallel ITE benchmark

The opt-in `RobddManager::ite_parallel()` method uses the shared
ThreadManager and up to two fork levels for independent ITE cofactor
computations. It shares the existing mutex-protected canonical
unique table and ITE cache; it does **not** use a second pool.
`ite()` remains the production-default sequential implementation.
**The first manual measurement found a severe regression**, not an acceleration:
at 8/10/12 variables with concurrency 2, 4 or 8, parallel ITE took
approximately 3.9–7.2 times the matched sequential duration. The
`ite_parallel()` API is **experimental and not recommended for these
workloads**. Leave the ordinary `ite()` implementation as the default.
Contended manager tables, task overhead, and duplicated concurrent
cache misses are possible explanations; the benchmark does not
attribute the slowdown to any one of them. No performance speedup is assumed.

The standalone `benchmark_robdd_parallel_ite` executable uses
Ariadne Benchmarking (`Parameter`, `Matrix`, `Runner`, `Report`)
to compare serial and parallel ITE at `--concurrency 0,1,2,4,8`
in one process. The `--bits` axis (default 8,10,12) controls
the size of deterministic truth-table ROBDD operands. Each measured
sample uses a fresh manager with three constructed input formulas,
so the timed ITE begins from a **cold computed table**. Pool setup,
manager/input construction, and the independent exhaustive Boolean
oracle and canonical-result checks remain outside the timer.

From the BDD repository root:

```bash
git pull --ff-only
git submodule update --init --recursive
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release -DBUILD_PYTHON_BINDINGS=OFF
cmake --build build-release --target benchmark_robdd_parallel_ite
./build-release/benchmarks/benchmark_robdd_parallel_ite \
  --bits 8,10,12 --concurrency 0,1,2,4,8 --samples 7
```

The results include absolute medians, paired same-configuration
speedup and efficiency, and machine-readable `ROBDD_PARALLEL_ITE`
records. Configurations above the machine's concurrency maximum
are reported as skipped. The experiment is **manual-only**: no
performance benchmark is executed in CI or registered with CTest.


### Manual ITE synchronization and scheduling diagnostics

The optional `RobddManager::ite_profiled(c,t,e,parallel,counters)`
performs the same Boolean operation as the production paths, but
collects explicitly **instrumented** per-call counters. The ordinary
`ite()` and `ite_parallel()` do not execute this instrumented
recursion and retain their existing hot paths.

`benchmark_robdd_parallel_ite` automatically performs **one
separate diagnostic sample** for each serial/parallel matrix case,
after all corresponding uninstrumented performance samples. Use
`--profile-samples N` to vary this count, or
`--profile-samples 0` to disable diagnostics. The output lines
`ROBDD_ITE_PROFILE` report cache hits, raced cache inserts,
per-category mutex acquisitions/contended acquisitions/estimated
waiting nanoseconds, task fork counts, submit nanoseconds, join
waiting nanoseconds and task-start delay nanoseconds.

Each diagnostic sample uses a **new manager with a cold ITE cache**,
and it independently checks both the canonical result and every
Boolean assignment. The diagnostic measurements are **not** the
unmodified algorithm's duration: polling `try_lock()`, reading
the clock, creating counter storage and instrumenting every
recursive call affect behavior. Mutex wait, task submit, task
start delay and join waiting are **sums over possibly overlapping
threads**, not additive wall-time components. A raced cache
insertion proves that two computations reached the same ITE key,
but does not quantify all duplicated subtree work.

Run manually in Release after building the benchmark:

```bash
./build-release/benchmarks/benchmark_robdd_parallel_ite \
  --bits 8,10,12 --concurrency 0,1,2,4,8 \
  --samples 7 --profile-samples 1
```

No performance benchmark is run as part of CI.


### Node-store concurrency experiment (2026-11-09; reverted)

A controlled experiment separated immutable node-vector reads behind a
`std::shared_mutex`, keeping unique table and ITE cache under their
existing manager mutex. It preserved canonical access ordering and added
a concurrent vector-growth/read/snapshot regression. However the
operator's manual benchmark measured substantial regressions, **including
the serial hot path**: at 12 variables and 4 workers, sequential ITE
increased from 0.371 ms to 0.461 ms (+24%), and parallel ITE increased
from 1.400 ms to 2.152 ms (+54%). Similar slowdowns occurred for
8 and 10 variables. The shared-node lock was therefore **reverted**.

The manager again uses the original **single `std::mutex`** to protect
nodes, vector reallocation, unique table and ITE cache. The independent
concurrency regression was retained to continue testing simultaneous
canonical construction, node inspection, evaluation and coherent
statistics. Diagnostics and Benchmarking remain available for further
manual experiments. See `doc/designs/bdd_design.md` for all measured
values and the lock-wait caveats.

Neither the default serial `ite()` nor the experimental
`ite_parallel()` should be regarded as improved by this discarded
experiment. No benchmark executes in CI.


### Batched ITE cache and cofactor snapshot (2026-11-09)

`RobddManager::ite()` and `ite_parallel()` now consult the
computed cache and snapshot all nonterminal operand nodes
under **one acquisition of the original single manager mutex**.
Top-variable selection and both cofactor pairs reuse these
immutable copied values; no lock is held during recursion or
task join. All canonical allocation and cache publication
remain synchronized by the same mutex, retaining thread safety.

The diagnostic API measures this same batched path, and the
new `node_snapshot_reads` output counts copied operand nodes.
Legacy separate-node-lock diagnostic metrics are zero for
this path; pre-change per-category lock counts must not be
compared directly. This is an experiment, **not yet a proven
speedup**: rerun the manual Release benchmark and check
both sequential and opt-in parallel timings. Correctness
tests and sanitizers run in CI, but performance benchmarks do not.
