# Ariadne BDD design

This is the living design manual for `ariadne-cps/bdd`. It also records the chronological implementation history and should be updated whenever a design decision changes, a milestone is implemented, a benchmark changes our direction, or an attempted approach fails.

## 1. Purpose

Ariadne BDD is a standalone C++20 container library for binary decision-diagram representations in the Ariadne ecosystem. The first concrete representation is ROBDD; additional families such as ZDD may be added later only when justified by real workloads.

Two downstream applications motivate the library:

- a BDD-backed SAT solver intended for integration with the Boolean layer of `ariadne-cps/kernel`;
- a concurrent canonical representation for binary N-tree/grid-paving structures, replacing the BuDDy-backed `BDDTreeSet` approach used in `ariadne-cps/release-1.0`.

The BDD library itself must not depend on Kernel and should remain reusable by other Ariadne repositories.

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

### ROBDD handles

`Robdd` is a lightweight non-owning handle consisting conceptually of:

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

Complemented edges are a planned optional representation policy rather than a mandatory property of every manager.

The main motivation is SAT/general-purpose symbolic work: representing `not f` by toggling a complement bit on a semantic node reference can make negation nearly free and allow a function and its complement to share the same physical nodes.

The N-tree/grid-paving case is more nuanced. Traversal-heavy operations such as iteration, enabled-cell counting, subset/disjoint checks and geometric descent may execute very little work per visited node. Requiring every traversal step to propagate edge polarity can therefore introduce measurable overhead even if it is implemented with masking/XOR rather than a branch.

Conversely, grid-set operations involving complement-like structure, especially difference/remove, may benefit from complemented edges through lower node counts and cheaper negation. This tradeoff must be measured rather than assumed.

The intended design is therefore orthogonal policy selection:

```text
ordering policy:
    fixed
    reorderable

edge policy:
    regular
    complemented
```

The initial expected configurations are:

```text
N-tree/grid-paving:
    fixed ordering
    regular edges initially

SAT/general-purpose:
    reorderable ordering optionally
    complemented edges
```

N-tree benchmarks should later compare regular and complemented edges on representative operations before deciding whether complemented edges are worth enabling there.

The implementation should distinguish physical node identity from semantic node reference:

```text
NodeId  = physical canonical node identity
NodeRef = NodeId plus representation metadata such as a complement bit
```

This separation should be established before complemented edges are implemented. The public API already avoids exposing raw node identifiers, which leaves room for this change without altering user-facing handle semantics.

As with ordering policy, the fixed/regular fast path should not repeatedly branch on whether complemented edges are enabled. Internal policy specialization or another zero/near-zero-overhead mechanism is preferred.

### Garbage collection

Not implemented.

The manager is currently append-only. Concurrent reclamation would require substantially stronger lifetime machinery; the first scalable versions will therefore prioritize bounded caches and explicit manager lifetime over node reclamation.

### Dynamic variable reordering

Not implemented.

Dynamic variable reordering is a planned optional manager capability rather than a separate BDD abstraction.

The design should keep one public BDD API and one semantic model while allowing two internal ordering policies:

```text
common BDD algorithms
       |
       +-- fixed ordering
       |     |
       |     +-- N-tree/grid-paving storage
       |
       +-- dynamic ordering
             |
             +-- SAT/general-purpose BDD workloads
```

The fixed-order path should not pay a branch or permutation lookup on every hot recursive step merely because dynamic reordering exists elsewhere in the library. An implementation based on internal policy specialization, or another equivalent zero/near-zero-overhead mechanism, is preferred over repeatedly checking a runtime `reordering_enabled` flag inside ITE/apply/quantification.

Before dynamic reordering is introduced, the implementation must separate stable logical variable identity from ordering level:

```text
BddVariable identity != ordering level
```

For a fixed-order manager, identity and level may use the same numeric representation as a fast path. For a reorderable manager, the implementation may maintain variable-to-level and level-to-variable mappings, or store the current level directly in nodes while retaining a separate logical-identity mapping. The latter should be evaluated because a reorder already requires rebuilding/recanonicalizing affected structure and it may keep normal recursive comparisons direct.

The public `Robdd` type remains common to fixed-order and reorderable ROBDD modes. The choice of ordering policy should be made when the manager/context is created and should not require distinct `FixedRobdd` and `DynamicRobdd` user-facing types.

Dynamic reordering also has a concurrency consequence. Normal BDD operations should remain concurrent, whereas a reorder is expected to require an exclusive manager phase: active operations must reach a safe point, the ordering and canonical structures are rebuilt, computed caches are invalidated as needed, and concurrent operations resume afterwards.

Reordering will only be implemented after the concurrent core is stable and benchmarked, and enabling it must be explicit.

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

## 8. N-tree and grid-paving representation

A second primary use case is the internal representation of binary N-trees and grid pavings.

Historical reference:

- `ariadne-cps/release-1.0/src/bdd_set.cpp` implements `BDDTreeSet` on top of BuDDy;
- `ariadne-cps/release-1.0/src/grid_set.cpp` implements the analogous structure explicitly with mutable `BinaryTreeNode` objects.

The BDD representation is attractive because ROBDD reduction performs structural recombination automatically. A skipped variable represents identical descendants, and a terminal `true` or `false` can represent an entire uniform subtree. In the historical BDD implementation, `mince` therefore changes the logical interpretation depth without physically expanding the diagram, while `recombine` simply drops that artificial mince depth. The explicit grid tree instead allocates/splits nodes and must explicitly recombine equal leaf subtrees.

The historical BuDDy-backed implementation requires more than ordinary Boolean operators. The new BDD library must eventually provide equivalents for:

- terminal tests and canonical equality;
- inspection of the top variable and low/high cofactors;
- construction from a variable and two branches;
- support extraction;
- order-preserving variable shifts and general renaming/replacement;
- existential quantification;
- a fused conjunction-plus-existential operation analogous to BuDDy's `bdd_appex(..., bddop_and, ...)`, because projection uses this pattern;
- efficient traversal in the presence of skipped variables.

Variable indices correspond naturally to N-tree subdivision levels. Fixed increasing variable order is therefore part of the path semantics for this use case.

The general BDD library may support dynamic variable reordering, but N-tree/grid-paving storage must use a manager/context configured with fixed natural ordering. That fixed configuration should use the dedicated fast path and should make reordering unavailable rather than merely relying on the caller never to request it.

### Manager ownership and concurrency

N-tree sets that participate in union, intersection, difference or projection must share one canonical BDD universe. A separate manager per set would make ordinary set algebra impossible without an expensive cross-manager import operation.

The intended model is therefore:

```text
shared RobddManager / BddContext
       |
       +-- NTree A -> immutable Robdd root
       +-- NTree B -> immutable Robdd root
       +-- NTree C -> immutable Robdd root
```

The immutable BDD roots are suitable for concurrent reads and snapshot-style updates. Higher-level mutable N-tree objects may replace their root after an operation, but the BDD nodes themselves remain immutable.

This use case strengthens the requirement that one shared manager scale under concurrent operations. The current single mutex is only a correctness baseline; sharded unique/computed tables and structured parallel recursion are required before claiming an advantage over BuDDy.

Garbage collection and lifetime become more important for long-running paving computations than for short SAT benchmarks. Reclamation must nevertheless be introduced only after the concurrent node/publication model is stable.

### Migration target

A future Ariadne grid-set integration should reproduce the semantic behavior of the historical `BDDTreeSet`, not its BuDDy API. The geometric/grid layer should depend on Ariadne BDD abstractions rather than exposing BuDDy-like global functions.

Compatibility tests should be derived from the historical `test_bdd_set.cpp` cases and compare, where applicable, against the explicit `GridTreeSet` implementation for:

- empty/full sets;
- height increase/minimization;
- mince/recombine semantics;
- union/intersection/difference;
- subset/disjoint/overlap predicates;
- enabled-cell iteration;
- projection to selected dimensions.

## 9. Choice of decision-diagram family

ROBDD remains the primary representation for Ariadne BDD because it is the strongest common fit for the two current target workloads: symbolic SAT/model-set manipulation and N-tree/grid-paving storage.

### N-tree/grid-paving storage

For N-tree storage, ROBDD reduction matches the tree semantics directly:

- if both branches of a subdivision are identical, the corresponding variable level disappears;
- a terminal `true` represents a uniformly enabled subtree;
- a terminal `false` represents a uniformly empty subtree;
- skipped variables therefore mean that both choices lead to the same represented subtree.

This is exactly the compression behavior exploited by the historical BuDDy-backed `BDDTreeSet`.

N-tree storage should therefore use an ROBDD manager with fixed natural variable ordering and dynamic reordering disabled.

### SAT and symbolic model sets

For SAT, ROBDD is not expected to replace CDCL as the universally best representation when the only question is satisfiable/unsatisfiable. Its value is instead that the root represents the complete satisfying assignment set and supports canonical Boolean operations, restriction, quantification, projection, model extraction/counting and incremental intersection.

The preferred SAT/general-purpose configuration is therefore an ROBDD with optional dynamic variable reordering. Complemented edges should be evaluated as an important optimization because they preserve ROBDD semantics while allowing a function and its complement to share structure.

### ZDD

Zero-suppressed decision diagrams are potentially attractive for sparse families of sets or satisfying assignments where relatively few variables are present/true in each member.

They are not a drop-in replacement for the N-tree representation: an omitted variable in an ROBDD means that both variable choices lead to the same function, while an omitted variable in a ZDD carries zero-suppression semantics. The ROBDD interpretation is the one required by grid-tree subdivision/recombination.

ZDD should therefore remain a possible future second representation, primarily for sparse AllSAT/combinatorial workloads, and should only be implemented if benchmark evidence demonstrates a substantial advantage.

### FBDD and SDD

Free BDDs can be smaller than ordered BDDs for some functions because different paths may use different variable orders. That flexibility weakens the simple global-order canonicity and makes apply, equivalence, concurrent unique-table management and N-tree path semantics less suitable for the current Ariadne goals.

Sentential Decision Diagrams can provide more flexible canonical decompositions and may outperform OBDDs on some knowledge-compilation workloads, but they require substantially more complex decomposition/vtree machinery and do not map naturally to N-tree subdivision levels.

Neither FBDD nor SDD is currently justified as the primary Ariadne representation.

### MTBDD / ADD

Multi-terminal BDDs or algebraic decision diagrams become relevant if grid leaves later carry values richer than enabled/disabled state, such as labels, occupancy values, costs or probabilities.

The current grid-set and SAT use cases are Boolean, so multi-terminal semantics should not complicate the initial ROBDD core. The design of reusable low-level infrastructure such as node arenas, concurrent tables and scheduling should nevertheless avoid needless assumptions that would make a future multi-terminal sibling implementation impossible.

### Public naming

The public class name should make the chosen decision-diagram semantics explicit.

The preferred direction is to rename the current representation handle from `Bdd` to `Robdd`, and correspondingly `RobddManager` to `RobddManager`. This has two advantages:

- users immediately know that the representation is reduced and ordered rather than an unspecified BDD variant;
- a future `Zdd`, `MtBdd` or other sibling representation can coexist without forcing a later semantic rename of a widely used generic `Bdd` type.

The logical variable abstraction is less representation-specific. `BddVariable` may remain shared for now, or be generalized later if multiple decision-diagram families actually appear; it does not need to become `RobddVariable` merely for naming symmetry.

The repository/package name remains BDD because it is the container for multiple decision-diagram families. Generic concepts such as `BddVariable`, the `bdd` interface target, the installed `ariadne-bdd` library, the top-level `ariadne-bdd.hpp` header and the aggregate `BDD_SRC` object target remain representation-neutral.

### Architectural decision

The implementation should remain semantically specialized around ROBDD rather than introducing a generic abstraction for every decision-diagram family.

Reusable infrastructure may be shared in the future, but ROBDD reduction, unique-table semantics and algorithms should remain explicit and optimizable.

The current preferred feature progression is:

```text
ROBDD core
    |
    +-- structural traversal
    +-- restriction / quantification / and_exists
    +-- complemented edges
    +-- fixed-order and reorderable manager policies
    +-- concurrent unique/computed tables
    +-- structured parallel recursion
```

Only after measurements on real SAT, AllSAT and grid-set workloads should an additional representation such as ZDD be considered.

## 10. Testing strategy

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

## 11. Benchmark strategy

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

## 12. Development history

### 2026-10-12 - ROBDD-specific naming and generic BDD split

The initial implementation was renamed so that representation-specific APIs and files explicitly state that they are ROBDDs, while the repository remains the generic BDD container.

Current structure:

- generic BDD layer: `include/bdd/variable.hpp` with `BddVariable`;
- ROBDD layer: `include/bdd/robdd.hpp` with `Robdd` and `RobddManager`;
- ROBDD implementation: `src/robdd.cpp`;
- aggregate source target remains `BDD_SRC` and currently contains the ROBDD implementation;
- package/repository targets remain `bdd` and `ariadne-bdd`;
- top-level `ariadne-bdd.hpp` remains the aggregate public header.

Representation-specific consumers were renamed consistently:

- C++ test: `test_robdd.cpp`;
- Python test: `test_robdd.py`;
- C++ tutorial: `tutorials/tutorial_robdd`;
- Python tutorial: `tutorial_robdd.py`;
- benchmark: `benchmark_robdd_nqueens.cpp`;
- Python binding code separates generic BDD-variable bindings from ROBDD bindings.

This leaves room for future files such as `zdd.hpp` / `zdd.cpp` to join the same BDD package and `BDD_SRC` target without overloading the meaning of a generic `Bdd` class.



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

### 2026-10-11 - Python bindings and tutorial parity

Added Python support following the standalone Ariadne binding pattern.

Build structure:

- `BUILD_PYTHON_BINDINGS` defaults to ON;
- `ss/python-common` supplies the shared pybind11 support layer;
- the standalone module is named `pyariadne`, consistent with the other Ariadne repositories;
- the module incorporates BDD and required transitive object code so that it is self-contained at link time;
- `PYTHON_ENFORCE_TESTS=ON` requires Python >= 3.11 and pytest.

Initial exported API:

- `BddManager`;
- `Bdd`;
- `BddVariable`;
- `BinaryWord`;
- constants, variables, negation, conjunction, disjunction, exclusive-or, ITE, evaluation and node count.

Python-created `Bdd` values keep their originating manager alive through pybind11 lifetime policies, matching the non-owning C++ handle contract.

Testing and tutorials:

- a small pytest smoke test checks canonical variables, commutative conjunction and the XOR truth table;
- `python/tutorials/tutorial_bdd.py` reproduces the C++ tutorial section-for-section;
- Unix and Windows CI install Python 3.13 and pytest, enforce Python tests, run C++ and Python tests separately, execute the standalone C++ tutorial and execute the Python tutorial;
- ThreadSanitizer explicitly disables Python bindings so the sanitizer job remains focused on the C++ concurrency core.

### 2026-10-11 - N-Queens benchmark

Added `benchmarks/benchmark_nqueens.cpp` and the standard Ariadne benchmark CMake target wiring.

The benchmark solves N-Queens for `n=5,6,7` by building the complete ROBDD of satisfying board assignments.

Encoding:

- one Boolean variable per board square;
- exactly one queen in each row;
- no two queens in the same column;
- no two queens on the same diagonal.

Each board size uses a fresh `BddManager`, so its timing is independent of unique/computed-table contents from previous runs.

Timing and reporting:

- elapsed construction time is measured with `Stopwatch<Microseconds>`;
- results are reported in milliseconds through Ariadne logging;
- the benchmark also records the manager's allocated non-terminal node count;
- a false final BDD is treated as an error, since N-Queens is satisfiable for 5, 6 and 7.

This benchmark intentionally measures full symbolic solution-set construction, not merely discovery of the first satisfying placement.

### 2026-10-11 - N-tree/grid-paving use case

The historical `BDDTreeSet` and `GridTreeSet` implementations in `ariadne-cps/release-1.0` were compared.

Key observations:

- `BDDTreeSet` uses BuDDy as a canonical compressed representation of an adaptive binary tree, not only as a Boolean formula package;
- missing BDD levels encode repeated identical subtrees, making structural recombination implicit in ROBDD reduction;
- the explicit `GridTreeSet` physically allocates, splits and recombines `BinaryTreeNode` objects;
- the BuDDy implementation relies on structural traversal (`var/low/high`), support extraction, variable replacement/shifting and existential quantification;
- projection uses a fused apply-and-existential operation followed by variable renaming;
- BuDDy's process-global state is incompatible with the desired concurrent use, whereas Ariadne BDD should support many concurrent N-tree operations sharing one manager.

Decision:

- concurrent N-tree/grid-paving representation is now a first-class use case of Ariadne BDD alongside SAT/SMT;
- fixed numeric variable order must be preservable as path semantics;
- the manager/context must be shareable by many sets;
- structural traversal, renaming and quantification move ahead of the SAT-specific layer in the implementation roadmap.

### 2026-10-11 - Fixed versus reorderable manager architecture

The reordering design was refined to avoid both unnecessary duplication and avoidable runtime overhead.

Decision:

- do not build two independent BDD implementations;
- retain one public `Bdd` API and one set of Boolean/structural semantics;
- allow distinct internal ordering policies for fixed and dynamically reorderable managers;
- the fixed-order implementation should have a specialized hot path with no repeated reordering flag checks and, where possible, no variable-to-level lookup;
- the reorderable implementation must distinguish logical variable identity from current ordering level;
- N-tree/grid-paving storage will always use the fixed natural-order manager and must not permit reordering;
- SAT/general-purpose BDD workloads may opt into dynamic reordering when measurements justify it;
- a dynamic reorder on a concurrent manager is expected to be an exclusive/quiescent phase rather than an operation interleaved freely with ordinary recursive BDD updates.

The preferred architecture is therefore policy separation inside one library, not separate `FixedBdd` and `DynamicBdd` user-facing types and not a per-operation runtime boolean branch.

### 2026-10-12 - ROBDD versus alternative decision-diagram families

The representation choice was reviewed against both target workloads rather than assuming that every BDD family is interchangeable.

Decision:

- ROBDD remains the primary Ariadne representation;
- N-tree/grid-paving storage uses fixed natural ordering because ROBDD skipped-variable semantics match implicit identical subtrees;
- SAT/general-purpose workloads may use dynamic reordering and should later evaluate complemented edges;
- ZDD is the most plausible future second representation for sparse model families or combinatorial enumeration, but only if benchmarks justify it;
- FBDD and SDD are not currently justified because their additional structural flexibility complicates canonicity, apply/concurrency machinery and N-tree mapping;
- MTBDD/ADD is deferred until a real non-Boolean leaf-value use case appears;
- do not generalize the ROBDD core prematurely into a multi-family decision-diagram framework.

This keeps optimization effort focused on the representation that serves both current applications while preserving room for evidence-driven extensions later.

### 2026-10-12 - Complemented-edge policy and explicit ROBDD naming

The complemented-edge design was refined after considering traversal-heavy grid-set workloads.

Decision:

- complemented edges are not mandatory for every manager;
- edge representation is an internal policy orthogonal to variable-ordering policy;
- SAT/general-purpose workloads are expected to benefit most from complemented edges;
- N-tree/grid-paving starts with regular edges and fixed natural order, then benchmarks complemented edges on real traversal, difference/remove and projection workloads;
- introduce an internal distinction between physical `NodeId` and semantic `NodeRef` before complemented edges;
- avoid per-node runtime feature checks in the regular-edge fast path.

The public naming direction was also clarified:

- prefer `Robdd` over the generic `Bdd` handle name;
- prefer `RobddManager` over `BddManager`;
- do not force `BddVariable` to become `RobddVariable`, because logical variable identity is not inherently tied to one decision-diagram family;
- perform the rename early, before substantial downstream integration.

### Next milestone

1. strengthen BDD tests with exhaustive/random truth tables;
2. add structural inspection APIs needed by N-tree traversal: top variable, low/high and support;
3. add order-preserving variable shifting/renaming and existential quantification, including an optimized `and_exists` path;
4. evaluate and implement complemented edges with a documented normalization rule;
5. make the Threading fork/join primitives concrete and implement them in `ariadne-cps/threading`;
6. add instrumentation for unique-table and computed-table behavior;
7. replace the coarse unique table with a sharded hash implementation;
8. parallelize ITE and the new structural operations above tunable grain thresholds;
9. add N-tree compatibility/benchmark workloads derived from the historical `BDDTreeSet` and `GridTreeSet`;
10. then begin the BDD-backed SAT layer on top of the now broader core API;
11. consider ZDD only after ROBDD benchmarks show a concrete sparse-model workload that warrants a second representation.

### 2026-10-27 - Align CI with the current Ariadne SMT matrix

The BDD CI was using Release-only build/test jobs and a ThreadSanitizer job
embedded in the Unix workflow, while SMT and Symbolic had moved to separate
Release-build and Debug-test jobs and independent sanitizer workflows.

Changes:
- Unix matrix now uses macOS 26 AppleClang 21, Ubuntu Clang 16 and GCC 13,
  each in Release and Debug; GCC Debug enables checked libstdc++ iterators.
- Windows 2025 likewise builds Release and executes Debug C++/Python tests.
- Release jobs install and validate both C++ and Python tutorials against the
  installed artifacts, including an explicit installed-module origin check.
- ThreadSanitizer and AddressSanitizer/UBSan run in a dedicated workflow.
- Coverage uses Debug and disables Python bindings to focus on the C++ layer.
- The standalone Python module installs to a prefix-relative site-packages
  location (Unix/macOS and Windows), matching the installation checks.

The ROBDD algorithm, public headers, tests, and dependency pins are unchanged.
The new CI configurations are not considered validated until GitHub Actions
has completed on this commit; compatibility and sanitizer results remain to
be confirmed by those runs.

## 13. Maintenance rule

Every substantial change should update this file in the same commit.

Each development entry should state:

- what was added or changed;
- which invariant or design decision motivated it;
- what tests or benchmarks passed;
- what failed or remains unverified;
- what the next concrete step is.

Failed approaches should remain documented rather than being silently removed; they are part of the design history.
