# Benchmarking registers this directory only for standalone builds.
# BDD's library bundle already contains Threading and Logging object files.
# Benchmarking is an interface dependency of this executable only.
add_executable(benchmark_robdd_nqueens benchmark_robdd_nqueens.cpp)
target_link_libraries(benchmark_robdd_nqueens PRIVATE
    ariadne-bdd bdd ariadne-benchmarking)
add_dependencies(benchmarks benchmark_robdd_nqueens)

# Opt-in internal ITE comparison; compile only, never run in CI.
add_executable(benchmark_robdd_parallel_ite benchmark_robdd_parallel_ite.cpp)
target_link_libraries(benchmark_robdd_parallel_ite PRIVATE
    ariadne-bdd bdd ariadne-benchmarking)
add_dependencies(benchmarks benchmark_robdd_parallel_ite)

# Manual-only unique-table insertion/lookup benchmark. No CTest registration.
add_executable(benchmark_robdd_unique_table benchmark_robdd_unique_table.cpp)
target_link_libraries(benchmark_robdd_unique_table PRIVATE
    ariadne-bdd bdd ariadne-benchmarking)
add_dependencies(benchmarks benchmark_robdd_unique_table)
