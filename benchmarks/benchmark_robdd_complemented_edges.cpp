/***************************************************************************
 *            benchmark_robdd_complemented_edges.cpp
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

#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <vector>

#include "logging/logging.hpp"
#include "threading/threading_command_line_interface.hpp"
#include "utility/stopwatch.hpp"
#include "bdd/robdd.hpp"
#include "bdd/detail/robdd_impl.hpp"

using namespace Ariadne;

namespace {

struct BenchmarkResult {
    const char* policy;
    SizeType functions;
    SizeType nodes_before_negation;
    SizeType nodes_after_negation;
    double negation_ms;
};

template<class Manager>
BenchmarkResult run_complement_closure(const char* policy) {
    constexpr SizeType variable_count = 12u;
    const SizeType function_count = (SizeType(1u) << variable_count) - 1u;

    Manager manager;
    using Diagram = typename Manager::Diagram;

    std::vector<Diagram> variables;
    variables.reserve(variable_count);
    for (SizeType i=0u; i!=variable_count; ++i) {
        variables.push_back(manager.variable(i));
    }

    // Build every non-empty conjunction of the same variable set. This phase
    // contains no explicit negations, so the timed phase below isolates the
    // cost of closing an already-built canonical family under complement.
    std::vector<Diagram> functions;
    functions.reserve(function_count);
    for (SizeType mask=1u; mask<=function_count; ++mask) {
        Diagram value = manager.constant(true);
        for (SizeType i=0u; i!=variable_count; ++i) {
            if ((mask & (SizeType(1u) << i)) != 0u) {
                value = manager.conjunction(value,variables[i]);
            }
        }
        functions.push_back(value);
    }

    const SizeType nodes_before_negation = manager.node_count();

    Stopwatch<Microseconds> stopwatch;
    std::vector<Diagram> complements;
    complements.reserve(functions.size());
    for (auto const& value : functions) {
        complements.push_back(manager.negation(value));
    }
    stopwatch.click();

    const SizeType nodes_after_negation = manager.node_count();
    const double negation_ms =
        static_cast<double>(stopwatch.duration().count())/1000.0;

    // Keep the semantic comparison honest and catch representation-specific
    // mistakes without including this verification in the timed interval.
    for (SizeType i=0u; i!=functions.size(); ++i) {
        if (manager.negation(complements[i]) != functions[i]) {
            throw std::runtime_error("Complement closure failed involution check.");
        }
    }

    return {
        policy,
        functions.size(),
        nodes_before_negation,
        nodes_after_negation,
        negation_ms
    };
}

Void print_results(
        BenchmarkResult const& regular,
        BenchmarkResult const& complemented)
{
    constexpr Int policy_width = 14;
    constexpr Int functions_width = 12;
    constexpr Int nodes_width = 15;
    constexpr Int extra_width = 13;
    constexpr Int time_width = 14;

    std::ostringstream table;
    table
        <<"ROBDD complemented-edge benchmark\n"
        <<std::left
        <<std::setw(policy_width)<<"policy"
        <<std::right
        <<std::setw(functions_width)<<"functions"
        <<std::setw(nodes_width)<<"nodes_before"
        <<std::setw(nodes_width)<<"nodes_after"
        <<std::setw(extra_width)<<"extra_nodes"
        <<std::setw(time_width)<<"negation_ms"
        <<"\n";

    for (auto const& result : {regular,complemented}) {
        table
            <<std::left
            <<std::setw(policy_width)<<result.policy
            <<std::right
            <<std::setw(functions_width)<<result.functions
            <<std::setw(nodes_width)<<result.nodes_before_negation
            <<std::setw(nodes_width)<<result.nodes_after_negation
            <<std::setw(extra_width)
            <<(result.nodes_after_negation-result.nodes_before_negation)
            <<std::setw(time_width)<<result.negation_ms
            <<"\n";
    }

    LOGGING_PRINTLN(table.str())
}

} // namespace

Int main(Int argc, const char* argv[]) {
    Logger::instance().configuration().set_verbosity(1u);

    ThreadingCommandLineInterface command_line;
    if (not command_line.acquire(argc,argv)) {
        return 0;
    }

    const auto regular =
        run_complement_closure<Detail::RegularRobddManager>("regular");
    const auto complemented =
        run_complement_closure<RobddManager>("complemented");

    print_results(regular,complemented);
    return 0;
}
