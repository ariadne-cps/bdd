# Ariadne BDD

[![License: GPL v3](https://img.shields.io/badge/License-GPL%20v3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![Unix](https://github.com/ariadne-cps/bdd/actions/workflows/unix.yml/badge.svg)](https://github.com/ariadne-cps/bdd/actions/workflows/unix.yml)
[![Windows](https://github.com/ariadne-cps/bdd/actions/workflows/win.yml/badge.svg)](https://github.com/ariadne-cps/bdd/actions/workflows/win.yml)
[![Coverage](https://github.com/ariadne-cps/bdd/actions/workflows/coverage.yml/badge.svg)](https://github.com/ariadne-cps/bdd/actions/workflows/coverage.yml)

Ariadne BDD is the standalone C++20 binary decision diagram layer for Ariadne. The current implementation provides a minimal reduced ordered BDD manager with canonical nodes, ITE-based Boolean operations, evaluation and a thread-safe baseline implementation.

The project is intentionally independent of Ariadne Kernel, Foundation, Numeric, Interval and Algebra. It depends directly on Ariadne Configuration and Ariadne Threading; Utility, Macro and Logging arrive through the Threading dependency chain.

## Current features

- Reduced ordered binary decision diagrams with variables ordered by increasing index.
- Canonical unique table.
- ITE computed table.
- Constants, variables, negation, conjunction, disjunction and exclusive-or.
- Evaluation under a total binary assignment.
- Thread-safe manager operations using a coarse-grained mutex baseline.
- A concurrent canonical-construction regression test.
- Unix, Windows, coverage and ThreadSanitizer CI configurations.

The current unique table and computed table are deliberately simple ordered maps protected by one mutex. They are a correctness baseline, not the intended final concurrent implementation.

## Dependencies

The repository contains these direct Git submodules:

- `ss/configuration` -> [ariadne-cps/configuration](https://github.com/ariadne-cps/configuration)
- `ss/threading` -> [ariadne-cps/threading](https://github.com/ariadne-cps/threading)

Clone recursively so that Threading's own dependencies are also initialized.

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

A C++20 compiler and CMake are required.

## ThreadSanitizer

```bash
mkdir build-tsan
cd build-tsan
cmake .. -DCMAKE_BUILD_TYPE=Debug -DTHREAD_SANITIZER=ON
cmake --build . --parallel
ctest --output-on-failure
```

## Design journal

The living design document and development journal is maintained in:

- `doc/designs/bdd_design.md`

It records architectural decisions, implemented milestones, known limitations, failed approaches and the next planned steps.

## License

Ariadne BDD is released under the GNU General Public License v3.0.
