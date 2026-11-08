# Benchmarking registers this directory only for standalone builds.
# BDD's library bundle already contains Threading and Logging object files.
# Benchmarking is an interface dependency of this executable only.
add_executable(benchmark_robdd_nqueens benchmark_robdd_nqueens.cpp)
target_link_libraries(benchmark_robdd_nqueens PRIVATE
    ariadne-bdd bdd ariadne-benchmarking)
add_dependencies(benchmarks benchmark_robdd_nqueens)
