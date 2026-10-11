set(BENCHMARKS
    benchmark_nqueens
)

foreach(BENCHMARK ${BENCHMARKS})
    add_executable(${BENCHMARK} ${BENCHMARK}.cpp)
    target_link_libraries(${BENCHMARK} PRIVATE ariadne-bdd bdd)
endforeach()

add_dependencies(benchmarks ${BENCHMARKS})
