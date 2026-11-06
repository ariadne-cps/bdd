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
#include <future>
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
    ARIADNE_TESTING_ADD_TEST(TestRobdd, test_complemented_edge_sharing);
    ARIADNE_TESTING_ADD_TEST(TestRobdd, test_evaluation);
    ARIADNE_TESTING_ADD_TEST(TestRobdd, test_manager_separation);
    ARIADNE_TESTING_ADD_TEST(TestRobdd, test_concurrent_canonical_construction);
    return ARIADNE_TESTING_RUN_TESTS_WITH_ARGS(argc, argv);
}
