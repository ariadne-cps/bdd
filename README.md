# Ariadne BDD

[![License: GPL v3](https://img.shields.io/badge/License-GPL%20v3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![Unix](https://github.com/ariadne-cps/bdd/actions/workflows/unix.yml/badge.svg)](https://github.com/ariadne-cps/bdd/actions/workflows/unix.yml)
[![Windows](https://github.com/ariadne-cps/bdd/actions/workflows/win.yml/badge.svg)](https://github.com/ariadne-cps/bdd/actions/workflows/win.yml)
[![Coverage](https://github.com/ariadne-cps/bdd/actions/workflows/coverage.yml/badge.svg)](https://github.com/ariadne-cps/bdd/actions/workflows/coverage.yml)
[![codecov](https://codecov.io/gh/ariadne-cps/bdd/branch/main/graph/badge.svg)](https://codecov.io/gh/ariadne-cps/bdd)

Ariadne BDD is the standalone C++20 binary decision diagram layer for Ariadne. The repository is a container for decision-diagram implementations; the current implementation is an ROBDD with canonical nodes, ITE-based Boolean operations, evaluation and a thread-safe baseline implementation.

The project is intentionally independent of Ariadne Kernel, Foundation, Numeric, Interval and Algebra. It depends directly on Ariadne Configuration and Ariadne Threading; Utility, Macro and Logging arrive through the Threading dependency chain.

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
- Unix, Windows, coverage and ThreadSanitizer CI configurations.

The current unique table and computed table are deliberately simple ordered maps protected by one mutex. They are a correctness baseline, not the intended final concurrent implementation.

## Dependencies

The repository contains these direct Git submodules:

- `ss/configuration` -> [ariadne-cps/configuration](https://github.com/ariadne-cps/configuration)
- `ss/threading` -> [ariadne-cps/threading](https://github.com/ariadne-cps/threading)
- `ss/python-common` -> [ariadne-cps/python-common](https://github.com/ariadne-cps/python-common), used only to build the Python bindings

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

A standalone C++ tutorial is available in `tutorials/tutorial_robdd`. It is designed to be built against an installed Ariadne BDD rather than directly against the repository source tree.

After installing Ariadne BDD to a prefix:

```bash
cmake -S tutorials/tutorial_robdd -B tutorial-build -DCMAKE_PREFIX_PATH=/path/to/install
cmake --build tutorial-build
./tutorial-build/tutorial_robdd
```

A matching Python tutorial is available in `python/tutorials/tutorial_robdd.py`. It reproduces the same sequence of examples using the Python bindings.

The Unix and Windows CI jobs build and execute both tutorials.

## ThreadSanitizer

```bash
mkdir build-tsan
cd build-tsan
cmake .. -DCMAKE_BUILD_TYPE=Debug -DTHREAD_SANITIZER=ON
cmake --build . --parallel
ctest --output-on-failure
```

## License

Ariadne BDD is released under the GNU General Public License v3.0.
