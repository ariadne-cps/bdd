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

#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "threading/threading_command_line_interface.hpp"
#include "ariadne-benchmarking.hpp"
#include "bdd/robdd.hpp"

using namespace Ariadne;
namespace Bench=Ariadne::Benchmarking;

namespace {

struct BenchmarkResult {
    SizeType n;
    Bench::Statistics timing;
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

// A measured sample builds one complete formula with a fresh manager.
// The manager is constructed outside timing, and satisfiability and
// allocation consistency are validated outside timing, even for warm-ups.
BenchmarkResult run_nqueens(SizeType n, unsigned samples,
                            Bench::Runner const& runner) {
    std::unique_ptr<RobddManager> manager;
    std::optional<Robdd> solution;
    std::optional<SizeType> expected_nodes;

    Bench::Samples const elapsed=runner.run(
        "nqueens/"+std::to_string(n),samples,
        [&] {
            solution.reset();  // Drop the old handle before its manager.
            manager=std::make_unique<RobddManager>();
        },
        [&] {
            solution.emplace(build_nqueens(*manager,n));
            Bench::keep_alive(*solution);
        },
        [&] {
            if(not solution || solution->is_false())
                throw std::runtime_error(
                    "Unexpected N-Queens UNSAT result for n="+std::to_string(n));
            SizeType const allocated=manager->node_count();
            if(expected_nodes && *expected_nodes!=allocated)
                throw std::runtime_error(
                    "Inconsistent ROBDD node count for n="+std::to_string(n));
            expected_nodes=allocated;
        });

    if(not expected_nodes)
        throw std::runtime_error("N-Queens benchmark did not run");
    return {n,Bench::summarize(elapsed),*expected_nodes};
}

void print_results(std::vector<BenchmarkResult> const& results,
                   Bench::CommandLine const& cli) {
    Bench::Report report("ROBDD N-Queens benchmark",cli.summary_fields());
    report.set_parameter_header(
        "One fresh manager per sample; times in nanoseconds per complete "
        "N-Queens construction; median shown separately in milliseconds.");
    report.set_dimension_columns({
        {"N",0u,Bench::TableAlignment::Right},
        {"Median (ms)",0u,Bench::TableAlignment::Right},
        {"Allocated nodes",0u,Bench::TableAlignment::Right}
    });
    report.set_table_options(Bench::TableOptions::ruled());

    for(auto const& result:results) {
        report.add_row({
            std::to_string(result.n),
            Bench::Table::number(result.timing.median/1e6),
            std::to_string(result.allocated_nodes)
        },result.timing);
    }
    report.print();
}

} // namespace

int main(int argc,char const* argv[]) {
    try {
        unsigned samples=1u;  // Preserve the original single measured run.
        Bench::CommandLine cli;
        ThreadManager::instance();
        cli.add(threading_command_line_contribution());
        cli.add(CommandLineContribution("robdd-nqueens-benchmark", {
            make_valued_argument_parser(
                "", "samples",
                "Number of measured N-Queens constructions per N (default: 1)",
                [&samples](String const& text)->VoidFunction {
                    unsigned const count=Bench::parse_unsigned<unsigned>(text);
                    if(count==0u)
                        throw std::invalid_argument("samples must be positive");
                    return [&samples,count] { samples=count; };
                })
        }));
        if(not cli.acquire(argc,argv)) return 0;

        Bench::Runner runner(cli,3u);
        std::vector<BenchmarkResult> results;
        results.reserve(3u);
        for(SizeType n=5u;n<=7u;++n)
            results.push_back(run_nqueens(n,samples,runner));
        print_results(results,cli);
        return 0;
    } catch(std::exception const& error) {
        LOGGING_PRINTLN("benchmark_robdd_nqueens: "<<error.what())
        return 1;
    }
}
