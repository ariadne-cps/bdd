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

#include "ariadne-benchmarking.hpp"
#include "bdd/robdd.hpp"
#include "threading/thread_manager.hpp"

using namespace Ariadne;
namespace Bench=Ariadne::Benchmarking;

namespace {
#if defined(ARIADNE_BDD_UNIQUE_TABLE_ORDERED)
constexpr char const* unique_table_variant="ordered";
#else
constexpr char const* unique_table_variant="hash";
#endif

struct RestoreConcurrency {
    ThreadManager& manager;
    SizeType initial;
    ~RestoreConcurrency() noexcept {
        try { manager.set_concurrency(initial); } catch (...) { }
    }
};

struct Sample {
    RobddManager manager;
    Robdd low;
    Robdd high;

    Sample(unsigned keys,bool existing)
        : manager(),
          low(manager.variable(static_cast<SizeType>(keys)+1u)),
          high(manager.variable(static_cast<SizeType>(keys)+2u)) {
        if(existing)
            for(unsigned i=0u;i<keys;++i)
                Bench::keep_alive(
                    manager.make_node(BddVariable(i),low,high));
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

void apply(Sample& sample,unsigned keys,bool scheduled) {
    auto make_range=[&sample](unsigned first,unsigned last) {
        for(unsigned i=first;i<last;++i)
            Bench::keep_alive(
                sample.manager.make_node(BddVariable(i),sample.low,sample.high));
    };
    if(not scheduled or ThreadManager::instance().concurrency()==0u) {
        make_range(0u,keys);
        return;
    }
    auto& manager=ThreadManager::instance();
    SizeType const workers=manager.concurrency();
    // Bound queued task count to the number of useful keys.
    SizeType const jobs=std::min<SizeType>(workers,keys);
    unsigned const chunk=(keys+static_cast<unsigned>(jobs)-1u)
        /static_cast<unsigned>(jobs);
    std::vector<std::future<void>> futures;
    futures.reserve(jobs);
    for(SizeType j=0u;j<jobs;++j) {
        unsigned const first=static_cast<unsigned>(j)*chunk;
        if(first>=keys) break;
        unsigned const last=std::min(keys,first+chunk);
        futures.push_back(manager.enqueue(
            [&,first,last] { make_range(first,last); }));
    }
    for(auto& future:futures) future.get();
}

Result measure(unsigned keys,bool existing,bool scheduled,unsigned samples,
               Bench::Runner const& runner,double serial_ns_per_key) {
    SizeType const concurrency=ThreadManager::instance().concurrency();
    std::string const workload=existing ? "existing" : "fresh";
    std::string const method=scheduled ? "scheduled" : "serial";
    std::unique_ptr<Sample> sample;
    auto const timings=runner.run(
        "bdd/unique/"+workload+"/keys"+std::to_string(keys)+"/"+
        method+"/workers"+std::to_string(concurrency),
        samples,
        [&] {sample=std::make_unique<Sample>(keys,existing);},
        [&] {apply(*sample,keys,scheduled);},
        [&] {
            if(sample->manager.node_count()!=static_cast<SizeType>(keys)+2u)
                throw std::runtime_error(
                    "Canonical key count changed or duplicated");
            auto const snapshot=sample->manager.statistics();
            if(snapshot.unique_table_entries!=snapshot.node_count)
                throw std::runtime_error(
                    "Canonical unique table does not match node count");
            // Repeating a completed lookup must never create a new node.
            auto check=sample->manager.make_node(
                BddVariable(keys/2u),sample->low,sample->high);
            auto same=sample->manager.make_node(
                BddVariable(keys/2u),sample->low,sample->high);
            if(check!=same or
               sample->manager.node_count()!=static_cast<SizeType>(keys)+2u)
                throw std::runtime_error("Repeated canonical lookup differs");
        },
        static_cast<double>(keys));
    auto const stats=Bench::summarize(timings);
    if(not std::isfinite(stats.median) or stats.median<=0.0)
        throw std::runtime_error("Invalid unique-table timing");
    return {keys,concurrency,workload,method,stats,
            serial_ns_per_key>0.0 ? serial_ns_per_key/stats.median : 1.0,
            static_cast<SizeType>(keys)+2u};
}

void report(std::vector<Result> const& results,Bench::CommandLine const& cli) {
    Bench::Report output(
        "ROBDD unique table: fresh canonical insertion and existing lookup",
        cli.summary_fields());
    output.set_parameter_header(
        "Median nanoseconds per public make_node call."
        " Prepared operand nodes and existing-key population excluded."
        " Scheduled mode includes enqueue and wait overhead."
        " All runs use one shared canonical manager per sample.");
    output.set_dimension_columns({
        {"Keys",0u,Bench::TableAlignment::Right},
        {"Concurrency",0u,Bench::TableAlignment::Right},
        {"Workload",0u,Bench::TableAlignment::Left},
        {"Method",0u,Bench::TableAlignment::Left},
        {"Median (ns/key)",0u,Bench::TableAlignment::Right},
        {"Speedup",0u,Bench::TableAlignment::Right},
        {"Nodes",0u,Bench::TableAlignment::Right}
    });
    output.set_table_options(Bench::TableOptions::ruled());
    for(auto const& row:results)
        output.add_row({
            std::to_string(row.keys),
            std::to_string(row.concurrency),
            row.workload,
            row.method,
            Bench::Table::number(row.timing.median),
            Bench::Table::number(row.speedup),
            std::to_string(row.physical_nodes)
        },row.timing);
    output.print();
    for(auto const& row:results) {
        LOGGING_PRINTLN("ROBDD_UNIQUE_TABLE unique_table="<<unique_table_variant
            <<" keys="<<row.keys
            <<" concurrency="<<row.concurrency
            <<" workload="<<row.workload
            <<" method="<<row.method
            <<" median_ns_per_key="<<std::setprecision(9)<<row.timing.median
            <<" speedup="<<row.speedup
            <<" nodes="<<row.physical_nodes)
    }
}

} // namespace

int main(int argc,char const* argv[]) {
    try {
        unsigned samples=7u;
        auto& threads=ThreadManager::instance();
        Bench::Parameter<unsigned> keys(
            "keys","Number of canonical node keys per sample",
            {256u,1024u,4096u},
            [](std::string const& token) {
                unsigned const n=Bench::parse_unsigned<unsigned>(String(token));
                if(n==0u or n>16384u)
                    throw std::invalid_argument("keys must be in 1..16384");
                return n;
            });
        Bench::Parameter<SizeType> concurrency(
            "concurrency","Pool sizes; comma-separated, 0 or max supported",
            {0u,1u,2u,4u,8u},
            [&threads](std::string const& token) {
                if(token=="max") return threads.maximum_concurrency();
                return Bench::parse_unsigned<SizeType>(String(token));
            });
        Bench::Matrix matrix;
        matrix.add(keys).add(concurrency);
        Bench::CommandLine cli;
        cli.add_parameters(matrix);
        cli.add(CommandLineContribution("robdd-unique-table",{
            make_valued_argument_parser(
                "","samples","Measured samples per configuration",
                [&samples](String const& token)->VoidFunction {
                    unsigned const value=Bench::parse_unsigned<unsigned>(token);
                    if(value==0u) throw std::invalid_argument("samples must be positive");
                    return [&samples,value] {samples=value;};
                })
        }));
        if(not cli.acquire(argc,argv)) return 0;
        RestoreConcurrency restore{threads,threads.concurrency()};
        Bench::Runner runner(cli,matrix.combinations()*4u);
        std::vector<Result> results;
        results.reserve(matrix.combinations()*4u);
        matrix.for_each([&](Bench::Configuration const& configuration) {
            unsigned const key_count=configuration.value(keys);
            SizeType const worker_count=configuration.value(concurrency);
            if(worker_count>threads.maximum_concurrency()) {
                LOGGING_PRINTLN("ROBDD_UNIQUE_TABLE_SKIPPED keys="<<key_count
                    <<" concurrency="<<worker_count
                    <<" maximum="<<threads.maximum_concurrency())
                return;
            }
            threads.set_concurrency(worker_count);
            runner.configuration_started(configuration);
            for(bool existing: {false,true}) {
                auto baseline=measure(key_count,existing,false,samples,runner,0.0);
                double const serial_ns=baseline.timing.median;
                results.push_back(std::move(baseline));
                results.push_back(measure(
                    key_count,existing,true,samples,runner,serial_ns));
            }
            runner.configuration_completed(configuration);
        });
        report(results,cli);
        return 0;
    } catch(std::exception const& error) {
        LOGGING_PRINTLN("benchmark_robdd_unique_table: "<<error.what())
        return 1;
    }
}
