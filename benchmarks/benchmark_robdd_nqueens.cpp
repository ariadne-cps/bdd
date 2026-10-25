/***************************************************************************
 *            benchmark_nqueens.cpp
 *
 *  Copyright  2026  Luca Geretti
 ****************************************************************************/

/*
 *  This file is part of Ariadne BDD.
 *
 *  Ariadne BDD is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 */

#include <sstream>
#include <stdexcept>
#include <vector>

#include "logging/logging.hpp"
#include "threading/threading_command_line_interface.hpp"
#include "threading/thread_manager.hpp"
#include "utility/stopwatch.hpp"
#include "bdd/robdd.hpp"

using namespace Ariadne;

namespace {

struct BenchmarkResult {
    SizeType n;
    double elapsed_ms;
    SizeType allocated_nodes;
};

SizeType index(SizeType n, SizeType row, SizeType column) {
    return row*n+column;
}

Robdd forbid_pair(
        RobddManager& manager,
        Robdd const& first,
        Robdd const& second)
{
    return manager.disjunction(
        manager.negation(first),
        manager.negation(second));
}

Robdd build_nqueens(RobddManager& manager, SizeType n) {
    std::vector<Robdd> square;
    square.reserve(n*n);

    for(SizeType row=0u; row!=n; ++row) {
        for(SizeType column=0u; column!=n; ++column) {
            square.push_back(manager.variable(index(n,row,column)));
        }
    }

    Robdd solution=manager.constant(true);

    // Exactly one queen in each row.
    for(SizeType row=0u; row!=n; ++row) {
        Robdd at_least_one=manager.constant(false);

        for(SizeType column=0u; column!=n; ++column) {
            at_least_one=manager.disjunction(
                at_least_one,
                square[index(n,row,column)]);
        }

        solution=manager.conjunction(solution,at_least_one);

        for(SizeType first_column=0u; first_column!=n; ++first_column) {
            for(SizeType second_column=first_column+1u;
                    second_column!=n;
                    ++second_column) {
                solution=manager.conjunction(
                    solution,
                    forbid_pair(
                        manager,
                        square[index(n,row,first_column)],
                        square[index(n,row,second_column)]));
            }
        }
    }

    // Queens in different rows may not share a column or diagonal.
    for(SizeType first_row=0u; first_row!=n; ++first_row) {
        for(SizeType second_row=first_row+1u; second_row!=n; ++second_row) {
            const SizeType row_distance=second_row-first_row;

            for(SizeType first_column=0u; first_column!=n; ++first_column) {
                for(SizeType second_column=0u; second_column!=n; ++second_column) {
                    const SizeType column_distance=
                        first_column>second_column
                        ? first_column-second_column
                        : second_column-first_column;

                    if(first_column==second_column
                            or row_distance==column_distance) {
                        solution=manager.conjunction(
                            solution,
                            forbid_pair(
                                manager,
                                square[index(n,first_row,first_column)],
                                square[index(n,second_row,second_column)]));
                    }
                }
            }
        }
    }

    return solution;
}

BenchmarkResult run_nqueens(SizeType n) {
    LOGGING_PRINTLN_AT(1,
        "N-Queens benchmark start: n="<<n)

    RobddManager manager;
    Stopwatch<Microseconds> stopwatch;

    const Robdd solution=build_nqueens(manager,n);

    stopwatch.click();

    if(solution.is_false()) {
        LOGGING_PRINTLN(
            "N-Queens benchmark unexpectedly returned UNSAT for n="<<n)
        throw std::runtime_error("Unexpected N-Queens UNSAT result.");
    }

    const double elapsed_ms=
        static_cast<double>(stopwatch.duration().count())/1000.0;

    const SizeType allocated_nodes=manager.node_count();

    LOGGING_PRINTLN_AT(1,
        "N-Queens benchmark result: n="<<n
        <<", time="<<elapsed_ms<<" ms"
        <<", allocated_nodes="<<allocated_nodes)

    return {n,elapsed_ms,allocated_nodes};
}

Void print_results(std::vector<BenchmarkResult> const& results) {
    std::ostringstream table;
    table
        <<"ROBDD N-Queens benchmark\n"
        <<"n\ttime_ms\tallocated_nodes\n";

    for(auto const& result : results) {
        table
            <<result.n<<"\t"
            <<result.elapsed_ms<<"\t"
            <<result.allocated_nodes<<"\n";
    }

    LOGGING_PRINTLN(table.str())
}

} // namespace

Int main(Int argc, const char* argv[]) {
    Logger::instance().configuration().set_verbosity(1u);

    // The threading CLI includes logging options and attaches the thread
    // registry before acquiring scheduler and concurrency settings.
    ThreadingCommandLineInterface command_line;

    if (not command_line.acquire(argc,argv)) {
        return 0;
    }

    std::vector<BenchmarkResult> results;
    results.reserve(3u);

    for(SizeType n=5u; n<=7u; ++n) {
        results.push_back(run_nqueens(n));
    }

    print_results(results);
    return 0;
}
