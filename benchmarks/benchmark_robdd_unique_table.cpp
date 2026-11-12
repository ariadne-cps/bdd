/***************************************************************************
 *            benchmarks/benchmark_robdd_unique_table.cpp
 *
 *  Copyright  2026  Luca Geretti
 *
 ****************************************************************************/

/*
 *  This file is part of Ariadne BDD.
 *
 *  Ariadne BDD is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Ariadne BDD is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Ariadne BDD. If not, see <https://www.gnu.org/licenses/>.
 */

/** @file benchmarks/benchmark_robdd_unique_table.cpp
 *  @brief Benchmark canonical ROBDD unique table insertion and lookup.
 */

/*
 * Manual benchmark for canonical unique table insertion/lookup.
 *
 * The public make_node API is measured end-to-end (including validation),
 * not raw hash-table operations. Each sample starts from a new manager,
 * with its operand handles created outside the timed region. The fresh
 * case inserts previously unknown nodes; the existing case repeats
 * canonical lookups for nodes prepared outside the timed region.
 *
 * Time includes enqueue/get overhead for scheduled workloads, intentionally.
 */

#include <algorithm>
#include <cmath>
#include <future>
#include <iomanip>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "bdd/robdd.hpp"
#include "threading/thread_manager.hpp"

#include "ariadne-benchmarking.hpp"

using namespace Ariadne;
namespace Bench = Ariadne::Benchmarking;

namespace {
#if defined(ARIADNE_BDD_UNIQUE_TABLE_ORDERED)
constexpr const char* unique_table_variant = "ordered";
#else
constexpr const char* unique_table_variant = "hash";
#endif

struct RestoreConcurrency {
    ThreadManager& manager;
    SizeType initial;
    ~RestoreConcurrency() noexcept {
        try {
            manager.set_concurrency(initial);
        } catch (...) {
        }
    }
};

struct Sample {
    RobddManager manager;
    Robdd low;
    Robdd high;

    Sample(unsigned keys, bool existing)
        : manager(),
          low(manager.variable(static_cast<SizeType>(keys) + 1u)),
          high(manager.variable(static_cast<SizeType>(keys) + 2u)) {
        if (existing)
            for (unsigned i = 0u; i < keys; ++i)
                Bench::keep_alive(
                    manager.make_node(BddVariable(i), low, high));
    }
};

struct Result {
    unsigned keys;
    SizeType concurrency;
    std::string workload;
    std::string method;
    Bench::Statistics timing;
    double speedup;
    SizeType physical_nodes;
};

void apply(Sample& sample, unsigned keys, bool scheduled) {
    auto make_range = [&sample](unsigned first, unsigned last) {
        for (unsigned i = first; i < last; ++i)
            Bench::keep_alive(
                sample.manager.make_node(BddVariable(i), sample.low, sample.high));
    };
    if (not scheduled or ThreadManager::instance().concurrency() == 0u) {
        make_range(0u, keys);
        return;
    }
    auto& manager = ThreadManager::instance();
    const SizeType workers = manager.concurrency();
    // Bound queued task count to the number of useful keys.
    const SizeType jobs = std::min<SizeType>(workers, keys);
    const unsigned chunk = (keys + static_cast<unsigned>(jobs) - 1u) / static_cast<unsigned>(jobs);
    std::vector<std::future<void>> futures;
    futures.reserve(jobs);
    for (SizeType j = 0u; j < jobs; ++j) {
        const unsigned first = static_cast<unsigned>(j) * chunk;
        if (first >= keys)
            break;
        const unsigned last = std::min(keys, first + chunk);
        futures.push_back(manager.enqueue(
            [&, first, last] { make_range(first, last); }));
    }
    for (auto& future: futures)
        future.get();
}

Result measure(unsigned keys, bool existing, bool scheduled, unsigned samples,
               const Bench::Runner& runner, double serial_ns_per_key) {
    const SizeType concurrency = ThreadManager::instance().concurrency();
    const std::string workload = existing ? "existing" : "fresh";
    const std::string method = scheduled ? "scheduled" : "serial";
    std::unique_ptr<Sample> sample;
    const auto timings = runner.run(
        "bdd/unique/" + workload + "/keys" + std::to_string(keys) + "/" +
            method + "/workers" + std::to_string(concurrency),
        samples,
        [&] { sample = std::make_unique<Sample>(keys, existing); },
        [&] { apply(*sample, keys, scheduled); },
        [&] {
            if (sample->manager.node_count() != static_cast<SizeType>(keys) + 2u)
                throw std::runtime_error(
                    "Canonical key count changed or duplicated");
            const auto snapshot = sample->manager.statistics();
            if (snapshot.unique_table_entries != snapshot.node_count)
                throw std::runtime_error(
                    "Canonical unique table does not match node count");
            // Repeating a completed lookup must never create a new node.
            auto check = sample->manager.make_node(
                BddVariable(keys / 2u), sample->low, sample->high);
            auto same = sample->manager.make_node(
                BddVariable(keys / 2u), sample->low, sample->high);
            if (check != same or
                sample->manager.node_count() != static_cast<SizeType>(keys) + 2u)
                throw std::runtime_error("Repeated canonical lookup differs");
        },
        static_cast<double>(keys));
    const auto stats = Bench::summarize(timings);
    if (not std::isfinite(stats.median) or stats.median <= 0.0)
        throw std::runtime_error("Invalid unique-table timing");
    return {keys, concurrency, workload, method, stats,
            serial_ns_per_key > 0.0 ? serial_ns_per_key / stats.median : 1.0,
            static_cast<SizeType>(keys) + 2u};
}

void report(const std::vector<Result>& results, const Bench::CommandLine& cli) {
    Bench::Report output(
        "ROBDD unique table: fresh canonical insertion and existing lookup",
        cli.summary_fields());
    output.set_parameter_header(
        "Median nanoseconds per public make_node call."
        " Prepared operand nodes and existing-key population excluded."
        " Scheduled mode includes enqueue and wait overhead."
        " All runs use one shared canonical manager per sample.");
    output.set_dimension_columns({
        {           "Keys", 0u, Bench::TableAlignment::Right},
        {    "Concurrency", 0u, Bench::TableAlignment::Right},
        {       "Workload", 0u,  Bench::TableAlignment::Left},
        {         "Method", 0u,  Bench::TableAlignment::Left},
        {"Median (ns/key)", 0u, Bench::TableAlignment::Right},
        {        "Speedup", 0u, Bench::TableAlignment::Right},
        {          "Nodes", 0u, Bench::TableAlignment::Right}
    });
    output.set_table_options(Bench::TableOptions::ruled());
    for (const auto& row: results)
        output.add_row({std::to_string(row.keys),
                        std::to_string(row.concurrency),
                        row.workload,
                        row.method,
                        Bench::Table::number(row.timing.median),
                        Bench::Table::number(row.speedup),
                        std::to_string(row.physical_nodes)},
                       row.timing);
    output.print();
    for (const auto& row: results) {
        LOGGING_PRINTLN("ROBDD_UNIQUE_TABLE unique_table=" << unique_table_variant
                                                           << " keys=" << row.keys
                                                           << " concurrency=" << row.concurrency
                                                           << " workload=" << row.workload
                                                           << " method=" << row.method
                                                           << " median_ns_per_key=" << std::setprecision(9) << row.timing.median
                                                           << " speedup=" << row.speedup
                                                           << " nodes=" << row.physical_nodes)
    }
}

} // namespace

int main(int argc, const char* argv[]) {
    try {
        unsigned samples = 7u;
        auto& threads = ThreadManager::instance();
        Bench::Parameter<unsigned> keys(
            "keys", "Number of canonical node keys per sample",
            {256u, 1024u, 4096u},
            [](const std::string& token) {
                const unsigned n = Bench::parse_unsigned<unsigned>(String(token));
                if (n == 0u or n > 16384u)
                    throw std::invalid_argument("keys must be in 1..16384");
                return n;
            });
        Bench::Parameter<SizeType> concurrency(
            "concurrency", "Pool sizes; comma-separated, 0 or max supported",
            {0u, 1u, 2u, 4u, 8u},
            [&threads](const std::string& token) {
                if (token == "max")
                    return threads.maximum_concurrency();
                return Bench::parse_unsigned<SizeType>(String(token));
            });
        Bench::Matrix matrix;
        matrix.add(keys).add(concurrency);
        Bench::CommandLine cli;
        cli.add_parameters(matrix);
        cli.add(CommandLineContribution("robdd-unique-table", {make_valued_argument_parser(
                                                                  "", "samples", "Measured samples per configuration",
                                                                  [&samples](const String& token) -> VoidFunction {
                                                                      const unsigned value = Bench::parse_unsigned<unsigned>(token);
                                                                      if (value == 0u)
                                                                          throw std::invalid_argument("samples must be positive");
                                                                      return [&samples, value] { samples = value; };
                                                                  })}));
        if (not cli.acquire(argc, argv))
            return 0;
        RestoreConcurrency restore{threads, threads.concurrency()};
        Bench::Runner runner(cli, matrix.combinations() * 4u);
        std::vector<Result> results;
        results.reserve(matrix.combinations() * 4u);
        matrix.for_each([&](const Bench::Configuration& configuration) {
            const unsigned key_count = configuration.value(keys);
            const SizeType worker_count = configuration.value(concurrency);
            if (worker_count > threads.maximum_concurrency()) {
                LOGGING_PRINTLN("ROBDD_UNIQUE_TABLE_SKIPPED keys=" << key_count
                                                                   << " concurrency=" << worker_count
                                                                   << " maximum=" << threads.maximum_concurrency())
                return;
            }
            threads.set_concurrency(worker_count);
            runner.configuration_started(configuration);
            for (bool existing: {false, true}) {
                auto baseline = measure(key_count, existing, false, samples, runner, 0.0);
                const double serial_ns = baseline.timing.median;
                results.push_back(std::move(baseline));
                results.push_back(measure(
                    key_count, existing, true, samples, runner, serial_ns));
            }
            runner.configuration_completed(configuration);
        });
        report(results, cli);
        return 0;
    } catch (const std::exception& error) {
        LOGGING_PRINTLN("benchmark_robdd_unique_table: " << error.what())
        return 1;
    }
}
