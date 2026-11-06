/***************************************************************************
 *            test_robdd.cpp
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

#include <algorithm>
#include <array>
#include <cstdint>
#include <future>
#include <limits>
#include <vector>

#include "ariadne-testing.hpp"
#include "threading/thread_manager.hpp"
#include "threading/threading_command_line_interface.hpp"
#include "logging/logging.hpp"
#include "bdd/robdd.hpp"

using namespace Ariadne;

class TestRobdd : public Ariadne::Testing::ContextFixture {
  public:
    using ContextFixture::ContextFixture;
    void test_constants_and_variables() {
        RobddManager manager;
        auto f = manager.constant(false);
        auto t = manager.constant(true);
        auto x = manager.variable(0u);
        auto x_again = manager.variable(0u);

        ARIADNE_TESTING_PROVE(f.is_false());
        ARIADNE_TESTING_PROVE(t.is_true());
        ARIADNE_TESTING_PROVE(not x.is_terminal());
        ARIADNE_TESTING_PROVE(x == x_again);
        ARIADNE_TESTING_PROVE(f != t);
        ARIADNE_TESTING_EXPECT_EQUAL(manager.node_count(),1u);

        BddVariable first(0u);
        BddVariable first_again(0u);
        BddVariable second(1u);
        ARIADNE_TESTING_PROVE(first == first_again);
        ARIADNE_TESTING_PROVE(first != second);
        ARIADNE_TESTING_PROVE(first < second);

        ARIADNE_TESTING_PROVE(manager.ite(x,t,f) == x);
    }

    void test_boolean_identities() {
        RobddManager manager;
        auto f = manager.constant(false);
        auto t = manager.constant(true);
        auto x = manager.variable(0u);
        auto y = manager.variable(1u);
        auto not_x = manager.negation(x);

        ARIADNE_TESTING_PROVE(manager.conjunction(x,t) == x);
        ARIADNE_TESTING_PROVE(manager.disjunction(x,f) == x);
        ARIADNE_TESTING_PROVE(manager.conjunction(x,not_x).is_false());
        ARIADNE_TESTING_PROVE(manager.disjunction(x,not_x).is_true());
        ARIADNE_TESTING_PROVE(manager.conjunction(x,y) == manager.conjunction(y,x));
        ARIADNE_TESTING_PROVE(manager.disjunction(x,y) == manager.disjunction(y,x));
    }

    void test_ite_complement_cases() {
        RobddManager manager;
        auto f = manager.constant(false);
        auto t = manager.constant(true);
        auto x = manager.variable(0u);
        auto y = manager.variable(1u);

        auto complement = manager.ite(x,f,t);
        ARIADNE_TESTING_PROVE(complement == manager.negation(x));

        auto false_then = manager.ite(x,f,y);
        auto expected = manager.conjunction(manager.negation(x),y);
        ARIADNE_TESTING_PROVE(false_then == expected);
    }

    void test_complemented_edge_sharing() {
        RobddManager manager;
        auto x = manager.variable(0u);
        auto y = manager.variable(1u);
        auto formula = manager.conjunction(x,y);
        SizeType nodes_before_negation = manager.node_count();

        auto complement = manager.negation(formula);

        ARIADNE_TESTING_EXPECT_EQUAL(manager.node_count(),nodes_before_negation);
        ARIADNE_TESTING_PROVE(manager.negation(complement) == formula);

        auto not_x = manager.negation(x);
        auto not_y = manager.negation(y);
        SizeType nodes_before_equivalent_complement = manager.node_count();
        auto equivalent_complement = manager.disjunction(not_x,not_y);

        ARIADNE_TESTING_PROVE(equivalent_complement == complement);
        ARIADNE_TESTING_EXPECT_EQUAL(
            manager.node_count(),nodes_before_equivalent_complement);

        ARIADNE_TESTING_EXPECT_EQUAL(manager.evaluate(complement,make_binary_word("00")),true);
        ARIADNE_TESTING_EXPECT_EQUAL(manager.evaluate(complement,make_binary_word("01")),true);
        ARIADNE_TESTING_EXPECT_EQUAL(manager.evaluate(complement,make_binary_word("10")),true);
        ARIADNE_TESTING_EXPECT_EQUAL(manager.evaluate(complement,make_binary_word("11")),false);
    }

    void test_structural_inspection_and_reconstruction() {
        RobddManager manager;
        auto f = manager.constant(false);
        auto t = manager.constant(true);
        auto x = manager.variable(0u);
        auto z = manager.variable(3u);

        ARIADNE_TESTING_PROVE(not manager.top_variable(f).has_value());
        ARIADNE_TESTING_PROVE(not manager.top_variable(t).has_value());
        ARIADNE_TESTING_EXPECT_EQUAL(manager.top_variable(x)->index(),0u);

        auto rooted = manager.make_node(BddVariable(0u),f,z);
        ARIADNE_TESTING_EXPECT_EQUAL(manager.top_variable(rooted)->index(),0u);
        ARIADNE_TESTING_PROVE(manager.low(rooted) == f);
        ARIADNE_TESTING_PROVE(manager.high(rooted) == z);
        ARIADNE_TESTING_PROVE(manager.top_variable(manager.high(rooted))->index() == 3u);

        auto complement = manager.negation(rooted);
        SizeType count_before_inspection = manager.node_count();
        ARIADNE_TESTING_PROVE(manager.low(complement) == t);
        ARIADNE_TESTING_PROVE(manager.high(complement) == manager.negation(z));
        ARIADNE_TESTING_EXPECT_EQUAL(manager.node_count(),count_before_inspection);
        ARIADNE_TESTING_PROVE(
            manager.make_node(BddVariable(0u),manager.low(rooted),manager.high(rooted))
            == rooted);
        ARIADNE_TESTING_PROVE(
            manager.make_node(BddVariable(0u),manager.low(complement),manager.high(complement))
            == complement);

        ARIADNE_TESTING_PROVE(manager.make_node(BddVariable(0u),z,z) == z);
        ARIADNE_TESTING_PROVE(manager.make_node(BddVariable(0u),f,t) == x);
        ARIADNE_TESTING_PROVE(manager.make_node(BddVariable(0u),t,f) == manager.negation(x));

        ARIADNE_TESTING_EXPECT_EXCEPTION(manager.low(f));
        ARIADNE_TESTING_EXPECT_EXCEPTION(manager.high(t));
        ARIADNE_TESTING_EXPECT_EXCEPTION(manager.make_node(BddVariable(3u),f,z));
        ARIADNE_TESTING_EXPECT_EXCEPTION(manager.make_node(BddVariable(4u),z,f));
        ARIADNE_TESTING_EXPECT_EXCEPTION(manager.make_node(BddVariable(3u),z,z));
        ARIADNE_TESTING_EXPECT_EXCEPTION(
            manager.variable(std::numeric_limits<SizeType>::max()));

        RobddManager other;
        auto other_value = other.variable(4u);
        ARIADNE_TESTING_EXPECT_EXCEPTION(manager.top_variable(other_value));
        ARIADNE_TESTING_EXPECT_EXCEPTION(manager.low(other_value));
        ARIADNE_TESTING_EXPECT_EXCEPTION(manager.high(other_value));
        ARIADNE_TESTING_EXPECT_EXCEPTION(
            manager.make_node(BddVariable(0u),f,other_value));
    }

    void test_exhaustive_three_variable_truth_tables() {
        RobddManager manager;
        std::array<Robdd,3> variables {
            manager.variable(0u),manager.variable(1u),manager.variable(2u)
        };
        char const* const assignments[8] = {
            "000","100","010","110","001","101","011","111"
        };
        for (unsigned mask=0u; mask!=256u; ++mask) {
            auto shannon = [&](auto&& self, SizeType level, unsigned prefix) -> Robdd {
                if (level == variables.size()) {
                    return manager.constant(((mask >> prefix) & 1u) != 0u);
                }
                auto low = self(self,level+1u,prefix);
                auto high = self(self,level+1u,prefix | (1u << level));
                return manager.ite(variables[level],high,low);
            };
            Robdd expanded = shannon(shannon,0u,0u);
            Robdd dnf = manager.constant(false);
            for (unsigned assignment=0u; assignment!=8u; ++assignment) {
                if (((mask >> assignment) & 1u) == 0u) continue;
                Robdd cube = manager.constant(true);
                for (SizeType variable=0u; variable!=variables.size(); ++variable) {
                    Robdd literal = ((assignment >> variable) & 1u)
                        ? variables[variable]
                        : manager.negation(variables[variable]);
                    cube = manager.conjunction(cube,literal);
                }
                dnf = manager.disjunction(dnf,cube);
            }
            ARIADNE_TESTING_PROVE(expanded == dnf);
            for (unsigned assignment=0u; assignment!=8u; ++assignment) {
                ARIADNE_TESTING_EXPECT_EQUAL(
                    manager.evaluate(expanded,make_binary_word(assignments[assignment])),
                    ((mask >> assignment) & 1u) != 0u);
            }
        }
    }

    void test_randomized_four_variable_formulas() {
        RobddManager manager;
        struct Formula {
            Robdd root;
            unsigned truth_table;
        };
        std::vector<Formula> formulas {
            {manager.variable(0u),0xAAAAu},
            {manager.variable(1u),0xCCCCu},
            {manager.variable(2u),0xF0F0u},
            {manager.variable(3u),0xFF00u},
            {manager.constant(false),0u},
            {manager.constant(true),0xFFFFu}
        };
        char const* const assignments[16] = {
            "0000","1000","0100","1100","0010","1010","0110","1110",
            "0001","1001","0101","1101","0011","1011","0111","1111"
        };
        std::uint32_t state = 0x9e3779b9u;
        auto next = [&state]() {
            state ^= state << 13u;
            state ^= state >> 17u;
            state ^= state << 5u;
            return state;
        };
        for (unsigned iteration=0u; iteration!=160u; ++iteration) {
            Formula a = formulas[next() % formulas.size()];
            Formula b = formulas[next() % formulas.size()];
            Formula c = formulas[next() % formulas.size()];
            unsigned operation = next() % 5u;
            Robdd root = manager.constant(false);
            unsigned expected = 0u;
            switch (operation) {
                case 0u:
                    root = manager.negation(a.root);
                    expected = (~a.truth_table) & 0xFFFFu;
                    break;
                case 1u:
                    root = manager.conjunction(a.root,b.root);
                    expected = a.truth_table & b.truth_table;
                    break;
                case 2u:
                    root = manager.disjunction(a.root,b.root);
                    expected = a.truth_table | b.truth_table;
                    break;
                case 3u:
                    root = manager.exclusive_or(a.root,b.root);
                    expected = a.truth_table ^ b.truth_table;
                    break;
                default:
                    root = manager.ite(a.root,b.root,c.root);
                    expected = (a.truth_table & b.truth_table)
                        | ((~a.truth_table) & c.truth_table);
                    break;
            }
            expected &= 0xFFFFu;
            for (unsigned assignment=0u; assignment!=16u; ++assignment) {
                ARIADNE_TESTING_EXPECT_EQUAL(
                    manager.evaluate(root,make_binary_word(assignments[assignment])),
                    ((expected >> assignment) & 1u) != 0u);
            }
            formulas.push_back({root,expected});
        }
    }

    void test_evaluation() {
        RobddManager manager;
        auto x = manager.variable(0u);
        auto y = manager.variable(1u);
        auto formula = manager.exclusive_or(x,y);

        ARIADNE_TESTING_EXPECT_EQUAL(manager.evaluate(formula,make_binary_word("00")),false);
        ARIADNE_TESTING_EXPECT_EQUAL(manager.evaluate(formula,make_binary_word("01")),true);
        ARIADNE_TESTING_EXPECT_EQUAL(manager.evaluate(formula,make_binary_word("10")),true);
        ARIADNE_TESTING_EXPECT_EQUAL(manager.evaluate(formula,make_binary_word("11")),false);
        ARIADNE_TESTING_EXPECT_EXCEPTION(manager.evaluate(y,make_binary_word("0")));
    }

    void test_manager_separation() {
        RobddManager first;
        RobddManager second;
        auto x = first.variable(0u);
        auto y = second.variable(0u);

        ARIADNE_TESTING_PROVE(x != y);
        ARIADNE_TESTING_EXPECT_EXCEPTION(first.conjunction(x,y));
    }

    void test_concurrent_canonical_construction() {
        auto& thread_manager = ThreadManager::instance();
        SizeType maximum = thread_manager.maximum_concurrency();
        if (maximum == 0u) return;

        SizeType previous = thread_manager.concurrency();
        thread_manager.set_concurrency(std::min<SizeType>(maximum,4u));

        RobddManager manager;
        auto x = manager.variable(0u);
        auto y = manager.variable(1u);
        auto not_y = manager.negation(y);

        std::vector<std::future<Robdd>> futures;
        for (SizeType i = 0u; i != 32u; ++i) {
            futures.push_back(thread_manager.enqueue([&manager,x,y,not_y] {
                auto lhs = manager.conjunction(x,y);
                auto rhs = manager.conjunction(x,not_y);
                return manager.disjunction(lhs,rhs);
            }));
        }

        for (auto& future : futures) {
            ARIADNE_TESTING_PROVE(future.get() == x);
        }

        thread_manager.set_concurrency(previous);
    }

    void test_threading_command_line() {
        ThreadingCommandLineInterface cli;
        ARIADNE_TESTING_PROVE(cli.acquire({""}));
        ARIADNE_TESTING_PROVE(cli.acquire({"","--verbosity","2","--theme","none","--scheduler","immediate","--concurrency","0"}));
        ARIADNE_TESTING_EXPECT_EQUAL(Logger::instance().configuration().verbosity(),2u);
        ARIADNE_TESTING_EXPECT_EQUAL(ThreadManager::instance().concurrency(),0u);
        ARIADNE_TESTING_PROVE(not cli.acquire({"","--concurrency","invalid"}));
        Logger::instance().use_nonblocking_scheduler();
    }

};

int main(int argc, char** argv) {
    ARIADNE_TESTING_SUITE("test_robdd");
    ARIADNE_TESTING_ADD_TEST(TestRobdd, test_threading_command_line);
    ARIADNE_TESTING_ADD_TEST(TestRobdd, test_constants_and_variables);
    ARIADNE_TESTING_ADD_TEST(TestRobdd, test_boolean_identities);
    ARIADNE_TESTING_ADD_TEST(TestRobdd, test_ite_complement_cases);
    ARIADNE_TESTING_ADD_TEST(TestRobdd, test_complemented_edge_sharing);
    ARIADNE_TESTING_ADD_TEST(TestRobdd, test_structural_inspection_and_reconstruction);
    ARIADNE_TESTING_ADD_TEST(TestRobdd, test_exhaustive_three_variable_truth_tables);
    ARIADNE_TESTING_ADD_TEST(TestRobdd, test_randomized_four_variable_formulas);
    ARIADNE_TESTING_ADD_TEST(TestRobdd, test_evaluation);
    ARIADNE_TESTING_ADD_TEST(TestRobdd, test_manager_separation);
    ARIADNE_TESTING_ADD_TEST(TestRobdd, test_concurrent_canonical_construction);
    return ARIADNE_TESTING_RUN_TESTS_WITH_ARGS(argc, argv);
}
