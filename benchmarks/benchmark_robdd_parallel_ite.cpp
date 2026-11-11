/***************************************************************************
 *            benchmarks/benchmark_robdd_parallel_ite.cpp
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

/** @file benchmarks/benchmark_robdd_parallel_ite.cpp
 *  @brief Benchmark bounded parallel ROBDD if-then-else computation.
 */

/*
 * Manual benchmark of internal ROBDD ITE parallelism.
 * Uses Ariadne Benchmarking's typed concurrency/depth matrix.
 * Every timed sample starts from a fresh manager whose ITE cache is cold.
 */

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <memory>
#include <optional>
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

struct Result {
    unsigned bits;
    SizeType concurrency;
    std::string method;
    Bench::Statistics statistics;
    double speedup;
    SizeType node_count;
};

struct RestoreConcurrency {
    ThreadManager& manager;
    SizeType initial;
    ~RestoreConcurrency() noexcept {
        try { manager.set_concurrency(initial); } catch (...) { }
    }
};

bool truth_bit(SizeType assignment,SizeType salt) {
    std::uint64_t value=static_cast<std::uint64_t>(assignment)
        + 0x9e3779b97f4a7c15ULL*(salt+1u);
    value=(value^(value>>30u))*0xbf58476d1ce4e5b9ULL;
    value=(value^(value>>27u))*0x94d049bb133111ebULL;
    return ((value^(value>>31u))&1u)!=0u;
}

Robdd build_formula(RobddManager& manager,unsigned bits,SizeType salt) {
    auto visit=[&](auto&& self,unsigned level,SizeType assignment)->Robdd {
        if(level==bits) return manager.constant(truth_bit(assignment,salt));
        Robdd low=self(self,level+1u,assignment);
        Robdd high=self(self,level+1u,
                         assignment|(SizeType(1u)<<level));
        return manager.make_node(BddVariable(level),low,high);
    };
    return visit(visit,0u,0u);
}

struct Sample {
    // Handles must die before the manager. Members are destroyed in
    // reverse declaration order, so declare the owning manager first.
    RobddManager manager;
    std::optional<Robdd> condition;
    std::optional<Robdd> then_branch;
    std::optional<Robdd> else_branch;
    std::optional<Robdd> result;

    explicit Sample(unsigned bits) {
        condition.emplace(build_formula(manager,bits,1u));
        then_branch.emplace(build_formula(manager,bits,2u));
        else_branch.emplace(build_formula(manager,bits,3u));
    }
};

void verify(Sample& sample,unsigned bits) {
    // Independent Boolean oracle: each valuation must satisfy the
    // ternary conditional. This checks more than just cache identity.
    SizeType const count=SizeType(1u)<<bits;
    BinaryWord word;
    for(SizeType assignment=0u;assignment<count;++assignment) {
        word=BinaryWord{};
        for(unsigned bit=0u;bit<bits;++bit)
            word.push_back(((assignment>>bit)&1u)!=0u);
        bool expected=truth_bit(assignment,truth_bit(assignment,1u)?2u:3u);
        if(sample.manager.evaluate(*sample.result,word)!=expected)
            throw std::runtime_error("ITE truth-table oracle mismatch");
    }
    // Canonical identity with the existing sequential path.
    Robdd reference=sample.manager.ite(
        *sample.condition,*sample.then_branch,*sample.else_branch);
    if(not (*sample.result==reference))
        throw std::runtime_error("Parallel ITE differs from serial result");
}

Result measure(unsigned bits,bool parallel,unsigned samples,
               Bench::Runner const& runner,double serial_ns) {
    SizeType const concurrency=ThreadManager::instance().concurrency();
    std::string method=parallel ? "parallel" : "serial";
    std::unique_ptr<Sample> current;
    std::optional<SizeType> expected_nodes;
    auto observations=runner.run(
        "bdd/ite/bits"+std::to_string(bits)+
        "/concurrency"+std::to_string(concurrency)+"/"+method,
        samples,
        [&] {current=std::make_unique<Sample>(bits);},
        [&] {
            if(parallel)
                current->result.emplace(current->manager.ite_parallel(
                    *current->condition,*current->then_branch,*current->else_branch));
            else
                current->result.emplace(current->manager.ite(
                    *current->condition,*current->then_branch,*current->else_branch));
            Bench::keep_alive(*current->result);
        },
        [&] {
            if(not current or not current->result)
                throw std::runtime_error("ITE operation did not produce a result");
            SizeType before_oracle=current->manager.node_count();
            if(expected_nodes and *expected_nodes!=before_oracle)
                throw std::runtime_error("Non-deterministic BDD node count");
            expected_nodes=before_oracle;
            verify(*current,bits);
        });
    auto stats=Bench::summarize(observations);
    if(not std::isfinite(stats.median) or stats.median<=0.0)
        throw std::runtime_error("Invalid ITE median");
    return {bits,concurrency,method,stats,
        serial_ns>0.0 ? serial_ns/stats.median : 1.0,
        expected_nodes.value_or(0u)};
}

// A SECOND, UNTIMED diagnostic experiment. Instrumentation perturbs the
// ITE and must never contribute to Runner's performance samples.
void profile_ite(unsigned bits,SizeType concurrency,bool parallel,
                 unsigned repetitions) {
    std::string const method=parallel ? "parallel" : "serial";
    for(unsigned repeat=0u;repeat<repetitions;++repeat) {
        Sample sample(bits); // Same fresh/cold ITE setup as measured samples.
        RobddIteProfile counters;
        sample.result.emplace(sample.manager.ite_profiled(
            *sample.condition,*sample.then_branch,*sample.else_branch,
            parallel,counters));
        verify(sample,bits); // All assignments and canonical identity.

        std::uint64_t const lock_wait_ns=
            counters.cache_lock_wait_ns
            +counters.node_lock_wait_ns
            +counters.unique_lock_wait_ns;
        SizeType const lock_contentions=
            counters.cache_lock_contended
            +counters.node_lock_contended
            +counters.unique_lock_contended;
        SizeType const lock_calls=
            counters.cache_lock_calls
            +counters.node_lock_calls
            +counters.unique_lock_calls;
        LOGGING_PRINTLN("ROBDD_ITE_PROFILE unique_table="<<unique_table_variant
            <<" bits="<<bits
            <<" concurrency="<<concurrency
            <<" method="<<method
            <<" repeat="<<repeat
            <<" calls="<<counters.calls
            <<" cache_lookups="<<counters.cache_lookups
            <<" cache_hits="<<counters.cache_hits
            <<" node_snapshot_reads="<<counters.node_snapshot_reads
            <<" raced_cache_inserts="<<counters.cache_raced_inserts
            <<" lock_calls="<<lock_calls
            <<" lock_contentions="<<lock_contentions
            <<" lock_wait_ns_sum="<<lock_wait_ns
            <<" cache_lock_calls="<<counters.cache_lock_calls
            <<" cache_lock_contended="<<counters.cache_lock_contended
            <<" cache_lock_wait_ns="<<counters.cache_lock_wait_ns
            <<" node_lock_calls="<<counters.node_lock_calls
            <<" node_lock_contended="<<counters.node_lock_contended
            <<" node_lock_wait_ns="<<counters.node_lock_wait_ns
            <<" unique_lock_calls="<<counters.unique_lock_calls
            <<" unique_lock_contended="<<counters.unique_lock_contended
            <<" unique_lock_wait_ns="<<counters.unique_lock_wait_ns
            <<" tasks_forked="<<counters.tasks_forked
            <<" task_submit_ns_sum="<<counters.task_submit_ns
            <<" task_join_ns_sum="<<counters.task_join_ns
            <<" task_start_delay_ns_sum="<<counters.task_start_delay_ns
            <<" nodes="<<sample.manager.node_count())
    }
}

void print_report(std::vector<Result> const& results,
                  Bench::CommandLine const& cli) {
    Bench::Report report(
        "ROBDD internal ITE: serial versus bounded parallel cofactor evaluation",
        cli.summary_fields());
    report.set_parameter_header(
        "Per sample: fresh manager, prepared operands outside timing,"
        " cold ITE cache at entry, canonical + exhaustive Boolean oracle"
        " outside timing. --concurrency supports ordered lists.");
    report.set_dimension_columns({
        {"Bits",0u,Bench::TableAlignment::Right},
        {"Concurrency",0u,Bench::TableAlignment::Right},
        {"Method",0u,Bench::TableAlignment::Left},
        {"Median (ms)",0u,Bench::TableAlignment::Right},
        {"Speedup",0u,Bench::TableAlignment::Right},
        {"Efficiency",0u,Bench::TableAlignment::Right},
        {"Nodes after ITE",0u,Bench::TableAlignment::Right}
    });
    report.set_table_options(Bench::TableOptions::ruled());
    for(auto const& row:results)
        report.add_row({
            std::to_string(row.bits),
            std::to_string(row.concurrency),
            row.method,
            Bench::Table::number(row.statistics.median/1e6),
            Bench::Table::number(row.speedup),
            row.method=="serial" or row.concurrency<2u
                ? "baseline"
                : Bench::Table::number(row.speedup/static_cast<double>(row.concurrency)),
            std::to_string(row.node_count)
        },row.statistics);
    report.print();
    for(auto const& row:results) {
        LOGGING_PRINTLN("ROBDD_PARALLEL_ITE unique_table="<<unique_table_variant
            <<" bits="<<row.bits
            <<" concurrency="<<row.concurrency
            <<" method="<<row.method
            <<" median_ms="<<std::setprecision(9)<<row.statistics.median/1e6
            <<" speedup="<<row.speedup
            <<" efficiency="<<(row.method=="serial" or row.concurrency<2u
                ? 1.0 : row.speedup/static_cast<double>(row.concurrency))
            <<" nodes="<<row.node_count)
    }
}

} // namespace

int main(int argc,char const* argv[]) {
    try {
        unsigned samples=7u;
        unsigned profile_samples=1u;
        ThreadManager& threading=ThreadManager::instance();
        Bench::Parameter<unsigned> bits(
            "bits","Number of Boolean variables, 2..14",
            {8u,10u,12u},
            [](std::string const& token) {
                unsigned value=Bench::parse_unsigned<unsigned>(String(token));
                if(value<2u or value>14u)
                    throw std::invalid_argument("bits must be within 2..14");
                return value;
            });
        Bench::Parameter<SizeType> concurrency(
            "concurrency","ThreadManager pool sizes; 0=sequential, max=hardware",
            {0u,1u,2u,4u,8u},
            [&threading](std::string const& token) {
                if(token=="max") return threading.maximum_concurrency();
                return Bench::parse_unsigned<SizeType>(String(token));
            });
        Bench::Matrix matrix;
        matrix.add(bits).add(concurrency);

        Bench::CommandLine cli;
        cli.add_parameters(matrix);
        cli.add(CommandLineContribution("robdd-parallel-ite",{
            make_valued_argument_parser(
                "","samples","Measured ITE samples per case (default: 7)",
                [&samples](String const& token)->VoidFunction {
                    unsigned count=Bench::parse_unsigned<unsigned>(token);
                    if(count==0u)
                        throw std::invalid_argument("samples must be positive");
                    return [&samples,count] {samples=count;};
                }),
            make_valued_argument_parser(
                "","profile-samples",
                "Separate untimed instrumentation runs per method/configuration"
                " (default: 1, 0 disables)",
                [&profile_samples](String const& token)->VoidFunction {
                    unsigned count=Bench::parse_unsigned<unsigned>(token);
                    return [&profile_samples,count] {profile_samples=count;};
                })
        }));
        if(not cli.acquire(argc,argv)) return 0;

        RestoreConcurrency restore{threading,threading.concurrency()};
        Bench::Runner runner(cli,matrix.combinations()*2u);
        std::vector<Result> results;
        results.reserve(matrix.combinations()*2u);

        matrix.for_each([&](Bench::Configuration const& configuration) {
            unsigned const number_bits=configuration.value(bits);
            SizeType const workers=configuration.value(concurrency);
            if(workers>threading.maximum_concurrency()) {
                LOGGING_PRINTLN("ROBDD_PARALLEL_ITE_SKIPPED bits="<<number_bits
                    <<" concurrency="<<workers
                    <<" maximum="<<threading.maximum_concurrency())
                return;
            }
            threading.set_concurrency(workers);
            runner.configuration_started(configuration);
            auto serial=measure(number_bits,false,samples,runner,0.0);
            double const baseline_ns=serial.statistics.median;
            results.push_back(std::move(serial));
            results.push_back(measure(number_bits,true,samples,runner,baseline_ns));
            runner.configuration_completed(configuration);

            // Run diagnostics AFTER both uninstrumented timing series.
            // Use fresh managers again to preserve cold-cache semantics.
            if(profile_samples>0u) {
                profile_ite(number_bits,workers,false,profile_samples);
                profile_ite(number_bits,workers,true,profile_samples);
            }
        });
        print_report(results,cli);
        return 0;
    } catch(std::exception const& error) {
        LOGGING_PRINTLN("benchmark_robdd_parallel_ite: "<<error.what())
        return 1;
    }
}
