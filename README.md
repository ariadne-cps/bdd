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
No performance speedup is assumed.

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
