# Ariadne BDD design

This is the living design manual for `ariadne-cps/bdd`. It also records the chronological implementation history and should be updated whenever a design decision changes, a milestone is implemented, a benchmark changes our direction, or an attempted approach fails.

## 1. Purpose

Ariadne BDD provides a standalone C++20 implementation of reduced ordered binary decision diagrams (ROBDDs) for the Ariadne ecosystem.

The first downstream application is a BDD-backed SAT solver intended for integration with the Boolean layer of `ariadne-cps/kernel`. The BDD library itself must not depend on Kernel and should remain reusable by other Ariadne repositories.

The long-term goal is a genuinely concurrent BDD implementation. Concurrency is treated as an architectural property rather than as a collection of ad-hoc asynchronous calls.

## 2. Dependency boundary

Direct dependencies:

- `ss/configuration`
- `ss/threading`

Threading supplies its own transitive Utility, Macro and Logging dependencies.

The BDD core must not depend on:

- `foundation`
- `numeric`
- `interval`
- `algebra`
- `kernel`

This keeps the dependency direction suitable for future reuse by higher layers and avoids pulling numerical packages such as GMP and MPFR into a Boolean data-structure library.

## 3. Current public model

### Variables

`BddVariable` is an index wrapper. Variable ordering is currently fixed:

```text
0 < 1 < 2 < ...
```

A lower variable index appears closer to the root of an ordered diagram.

### BDD handles

`Bdd` is a lightweight non-owning handle consisting conceptually of:

```text
(manager, root node id)
```

A handle is valid only while its manager is alive. BDDs from different managers cannot participate in the same operation.

### Terminals

Two node identifiers are reserved:

```text
0 = false
1 = true
```

Non-terminal identifiers begin at 2.

### Reduction and canonicity

Every non-terminal node is identified by:

```text
(variable, low, high)
```

The manager enforces both ROBDD reduction rules:

1. if `low == high`, the node is eliminated and that child is returned;
2. otherwise the unique table contains at most one node for a given `(variable, low, high)` triple.

For a fixed variable order and within one manager, logically equivalent diagrams produced by the implemented operations therefore converge on the same canonical root.

## 4. Boolean operations

The primitive recursive operation is ITE:

```text
ITE(f, g, h) = if f then g else h
```

The current Boolean operations are expressed through ITE:

```text
not f     = ITE(f, false, true)
f and g   = ITE(f, g, false)
f or g    = ITE(f, true, g)
f xor g   = ITE(f, not g, g)
```

ITE memoization uses a computed table keyed by `(condition, then, else)`.

## 5. Concurrency model

### Current baseline

The first implementation is thread-safe but not yet scalable.

The node arena, unique table and ITE cache are protected by one manager mutex. Recursive ITE never holds the mutex while recursively descending, so separate operations can make progress, but accesses to the shared tables serialize.

This baseline is intentional. It gives us:

- a simple correctness oracle;
- deterministic invariants to test;
- a target against which later concurrent structures can be benchmarked;
- a way to run ThreadSanitizer before introducing fine-grained synchronization.

### Planned scheduler integration

BDD recursion should eventually use structured fork/join primitives from `ariadne-cps/threading`, rather than recursively enqueueing tasks and blocking on ordinary `std::future` objects.

The intended pattern is conceptually:

```text
fork one cofactor
compute the other locally
help the scheduler while joining
```

The Threading repository is expected to gain task handles, helping waits, task groups and eventually work stealing. The BDD layer will decide *when* a recursive problem is large enough to parallelize; Threading will decide *how* that task is scheduled.

## 6. Data structures: current and planned

### Node arena

Current: append-only `std::vector<Node>`.

Planned: retain stable integer node identifiers while moving storage to a chunked/concurrent arena if growth or reallocation becomes a measurable issue.

### Unique table

Current: `std::map<NodeKey, NodeId>` under the manager mutex.

Planned: a sharded hash table with lookup/intern performed under shard-local synchronization. Lock-free publication should only be attempted if benchmarks justify the added complexity.

### Computed table

Current: `std::map<IteKey, NodeId>` under the manager mutex.

Planned: bounded hash-based cache. Cache entries may eventually be lossy because a miss affects performance, not semantic correctness. Any concurrent design must validate the complete key before accepting a cached result.

### Complemented edges

Not implemented yet.

The public API deliberately does not expose the representation of `NodeId`, so a complement bit can be introduced later without changing user-facing BDD handles. Before implementation we must choose and document a normalization rule that preserves unique-table canonicity.

### Garbage collection

Not implemented.

The manager is currently append-only. Concurrent reclamation would require substantially stronger lifetime machinery; the first scalable versions will therefore prioritize bounded caches and explicit manager lifetime over node reclamation.

### Dynamic variable reordering

Not implemented.

The API must avoid promises that would prevent future reordering, but the first milestones use a fixed numeric order. Reordering will only be considered after the core concurrent implementation is stable and benchmarked.

## 7. SAT layer roadmap

The intended BDD-backed SAT solver will represent the set of all currently admissible assignments as a BDD.

Initial state:

```text
solution_set = true
```

Adding a clause:

```text
solution_set = solution_set AND clause_bdd
```

Then:

```text
solution_set == false  -> UNSAT
otherwise              -> extract a satisfying assignment
```

This representation is intended to support incremental clause addition naturally. A future push/pop interface can store previous immutable BDD roots.

The first Kernel integration should consume the existing CNF emitted by `SmtBooleanEncoding`. This permits direct comparison with Kernel's existing CDCL-style Boolean backend before considering a direct `ContinuousPredicate -> BDD` encoding.

## 8. Testing strategy

Every structural optimization must preserve the existing semantic tests.

Required test families:

1. constants and terminal identities;
2. variable canonicity;
3. Boolean algebra identities;
4. exhaustive truth-table checks for small formulas;
5. manager-separation errors;
6. concurrent construction of equivalent diagrams;
7. randomized formulas compared with exhaustive Boolean evaluation;
8. ThreadSanitizer stress tests;
9. performance benchmarks recording nodes, cache behavior and wall time.

Tests must not rely on numeric node identifiers being identical across separate managers or across future concurrent allocation schedules.

## 9. Benchmark strategy

The benchmark suite will eventually record:

- wall-clock time;
- allocated non-terminal nodes;
- unique-table hits/misses;
- computed-table hits/misses;
- peak memory;
- scaling from 1 to N worker threads.

Workloads should include:

- random Boolean formulas;
- random 3-SAT;
- pigeonhole formulas;
- mux/adders and other structured circuits;
- Boolean skeletons extracted from real Ariadne SMT problems.

A BDD backend is not expected to dominate CDCL on generic SAT instances. Its value must be demonstrated on structured formulas, repeated/incremental constraints, model-set operations and SMT theory-nogood refinement.

## 10. Development history

### 2026-10-11 - Repository bootstrap

Added:

- project structure following the standalone Ariadne repositories;
- CMake project and installation/test hooks;
- direct `configuration` and `threading` submodules;
- Unix, Windows and coverage workflows;
- ThreadSanitizer configuration;
- top-level `ariadne-bdd.hpp`;
- `BddVariable`, `Bdd` and `BddManager`;
- canonical reduced nodes;
- ITE and a computed table;
- negation, conjunction, disjunction and exclusive-or;
- total-assignment evaluation through Utility's `BinaryWord`;
- unit tests for identities, evaluation, manager separation and concurrent canonical construction.

What is expected to work at this point:

- creation of constants and ordered variables;
- canonical reuse of identical nodes inside one manager;
- Boolean construction through ITE;
- evaluation of diagrams;
- concurrent calls against one manager without unsynchronized table access.

What deliberately does not work yet:

- scalable parallel ITE;
- sharded/lock-free tables;
- complemented edges;
- garbage collection;
- dynamic variable ordering;
- quantification/restriction/composition;
- satisfying-assignment extraction;
- SAT/CNF API;
- Kernel integration.

Verification status:

- source-level bootstrap complete;
- CI is the first external compile/test verification for the new repository;
- performance has not yet been measured, so the coarse mutex and ordered maps must not be treated as acceptable final structures.

### 2026-10-11 - First CI failure: Utility include-order dependency

Observed:

- Unix release, ThreadSanitizer and coverage builds reached compilation and failed in `utility/binary_word.hpp`;
- `binary_word.hpp` uses `StringType` and `StringStream`, but those names were not visible when BDD included that header before `utility/typedefs.hpp`;
- the resulting parser errors also made the `make_binary_word("00")` test calls appear to have the wrong argument type.

Resolution:

- BDD now includes `utility/typedefs.hpp` before `utility/binary_word.hpp`;
- no change was made to Utility in this milestone, because the BDD repository should not mutate a transitive dependency merely to complete its bootstrap.

What this tells us:

- the initial ROBDD source itself passes a local strict-warning syntax build when the Ariadne types are supplied;
- the first CI failure was an integration/include-order problem, not yet evidence of a ROBDD algorithm failure.

Still to verify:

- full Unix build and tests after the include-order fix;
- Windows configuration failure, which occurred before compilation and may be independent;
- ThreadSanitizer runtime behavior.

### 2026-10-11 - Windows CMake source-directory failure

Observed:

- the Windows workflow reached the CMake configure step but reported that the build directory did not contain `CMakeLists.txt`;
- the command used `$GITHUB_WORKSPACE`, which is shell syntax valid in POSIX shells but not for PowerShell environment variables;
- because the expression expanded to no source argument, CMake fell back to the current working directory, which is the build directory.

Resolution:

- the Windows workflow now uses GitHub's shell-independent expression `${{ github.workspace }}`;
- configuration is explicit with `cmake -S "${{ github.workspace }}" -B .`.

This avoids depending on PowerShell's `$env:GITHUB_WORKSPACE` syntax and keeps the workflow easier to compare with the Unix configuration.

Still to verify:

- Windows configuration and compilation after this workflow fix;
- the BDD tests on Windows.

### 2026-10-11 - Windows workflow realigned with Threading

The previous Windows fix was functionally valid but did not follow the repository convention closely enough.

Decision:

- `ariadne-cps/threading/.github/workflows/win.yml` is the canonical template for BDD at this stage;
- BDD now copies that workflow structure directly;
- the only repository-specific source-path change is `${{runner.workspace}}/threading` -> `${{runner.workspace}}/bdd`;
- no vcpkg or Python setup is included, because BDD has neither Numeric dependencies nor Python bindings.

This supersedes the earlier ad-hoc Windows source-path fix. The lesson is to prefer an existing Ariadne sibling workflow over introducing a locally equivalent variant.

### 2026-10-11 - Bootstrap diagnostics removed before merge

The temporary `.github/workflows/diagnostic.yml` workflow has been removed.

It was introduced only to surface build/configuration logs while debugging the initial CI bootstrap and is not part of the repository's permanent CI design.

Permanent CI remains:

- Unix release builds;
- ThreadSanitizer;
- Windows release build;
- coverage.

The bootstrap branch is ready to be fast-forwarded into `main`.

### 2026-10-11 - Symbolic frontend boundary

Decision:

- the BDD repository remains independent of `ariadne-cps/kernel` and does not import Kernel's symbolic expression implementation;
- Kernel's existing `symbolic` layer will be reused later as the frontend for Ariadne expressions and predicates;
- the first integration path remains `ContinuousPredicate -> SmtBooleanEncoding -> BDD/SAT`, so the BDD backend can be compared fairly with the existing CDCL-style backend on the same Boolean encoding;
- after that baseline is established, a direct `ContinuousPredicate -> BDD` frontend may be added in Kernel to preserve Boolean structure and avoid unnecessary Tseitin variables when beneficial;
- BDD itself should not grow a second rich Boolean expression AST duplicating Kernel's symbolic facilities.

This keeps the core BDD representation index-based and reusable while preserving the richer named-variable, expression, substitution and traversal facilities already available in Kernel.

### 2026-10-11 - macOS Codecov upload restored

Observed:

- the BDD coverage workflow generated `coverage.info` successfully but stopped after the CMake `coverage` target;
- unlike the established Threading/Interval/Numeric/Algebra workflows, it did not invoke Codecov at all;
- the macOS dependency list also omitted `gnupg`, which is part of the working Codecov setup used by the sibling repositories.

Resolution:

- `.github/workflows/coverage.yml` is now aligned with the Threading coverage workflow;
- macOS installs `gnupg`;
- the workflow verifies that `CODECOV_TOKEN` is available;
- macOS uploads `${{runner.workspace}}/build/coverage.info` with `codecov/codecov-action@v7.1.1`;
- upload failures are configured to fail CI rather than being silently ignored.

Linux continues to build and generate coverage but does not upload it, matching the current Ariadne repository convention.

### 2026-10-11 - Workflow parity audit

A full comparison was made against the current workflows in Threading, Utility and Interval after the missing Codecov upload was discovered.

Relevant omission found:

- Unix and Windows builds compiled and tested BDD but did not exercise the install rules;
- most installable Ariadne libraries, including Utility, Foundation, Numeric, Interval, Algebra and Kernel, run `cmake --install` in their normal CI workflows;
- Threading currently does not, so using it as the only workflow template had hidden this check.

Resolution:

- Unix now installs Ariadne BDD to `${{runner.workspace}}/install` after tests;
- Windows performs the same install smoke test;
- this verifies that the public headers, library target and bundled dependency installation rules are actually usable.

Intentionally not copied from Interval:

- Python setup/tests, because BDD has no Python bindings;
- vcpkg/GMP/MPFR setup, because BDD has no Numeric dependency;
- tutorial build/run steps, because no BDD tutorial exists yet;
- the second "Verify Final Installation" pass used after tutorial checks, because BDD currently has no intermediate tutorial/install mutation that requires a second installation.

Other differences reviewed and retained:

- the Unix macOS runner version is not functionally required to match Interval's runner;
- the libffi-related `PKG_CONFIG_PATH` setup seen in some lightweight repositories is not required by the current BDD dependency graph;
- Coverage now matches the established lightweight Ariadne pattern, including macOS Codecov upload.

### 2026-10-11 - Standalone C++ tutorial

Added a standalone tutorial under `tutorials/tutorial_bdd`.

The tutorial is a consumer of an installed Ariadne BDD rather than of the repository source tree. It demonstrates:

- manager lifetime, constants and indexed variables;
- canonical reuse of variables and equivalent Boolean functions;
- conjunction, disjunction and negation;
- equivalence between an expanded multiplexer and `ITE(x,y,z)`;
- total-assignment evaluation through `BinaryWord`;
- the reduction rule `ITE(x,y,y) == y`.

CI now validates the installed-library usage on Unix and Windows:

1. install Ariadne BDD to a temporary prefix;
2. copy the tutorial outside the repository source tree;
3. configure it with `CMAKE_PREFIX_PATH` pointing to the install prefix;
4. build and execute the tutorial;
5. perform a final installation pass.

Source attribution was also normalized so that BDD source files created in this repository use `Luca Geretti` rather than `Ariadne contributors`.

### Next milestone

1. make the Threading fork/join primitives concrete and implement them in `ariadne-cps/threading`;
2. strengthen BDD tests with exhaustive/random truth tables;
3. add instrumentation for unique-table and computed-table behavior;
4. replace the coarse unique table with a sharded hash implementation;
5. parallelize ITE above a tunable grain threshold;
6. only then begin the BDD-backed SAT layer.

## 11. Maintenance rule

Every substantial change should update this file in the same commit.

Each development entry should state:

- what was added or changed;
- which invariant or design decision motivated it;
- what tests or benchmarks passed;
- what failed or remains unverified;
- what the next concrete step is.

Failed approaches should remain documented rather than being silently removed; they are part of the design history.
