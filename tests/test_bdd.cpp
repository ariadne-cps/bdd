/***************************************************************************
 *            test_bdd.cpp
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

#include "macro/testing.hpp"
#include "threading/thread_manager.hpp"
#include "bdd/bdd.hpp"

using namespace Ariadne;

class TestBdd {
  public:
    void test_constants_and_variables() {
        BddManager manager;
        auto f = manager.constant(false);
        auto t = manager.constant(true);
        auto x = manager.variable(0u);
        auto x_again = manager.variable(0u);

        ARIADNE_TEST_ASSERT(f.is_false())
        ARIADNE_TEST_ASSERT(t.is_true())
        ARIADNE_TEST_ASSERT(not x.is_terminal())
        ARIADNE_TEST_ASSERT(x == x_again)
        ARIADNE_TEST_ASSERT(f != t)
        ARIADNE_TEST_EQUALS(manager.node_count(),1u)

        BddVariable first(0u);
        BddVariable first_again(0u);
        BddVariable second(1u);
        ARIADNE_TEST_ASSERT(first == first_again)
        ARIADNE_TEST_ASSERT(first != second)
        ARIADNE_TEST_ASSERT(first < second)

        ARIADNE_TEST_ASSERT(manager.ite(x,t,f) == x)
    }

    void test_boolean_identities() {
        BddManager manager;
        auto f = manager.constant(false);
        auto t = manager.constant(true);
        auto x = manager.variable(0u);
        auto y = manager.variable(1u);
        auto not_x = manager.negation(x);

        ARIADNE_TEST_ASSERT(manager.conjunction(x,t) == x)
        ARIADNE_TEST_ASSERT(manager.disjunction(x,f) == x)
        ARIADNE_TEST_ASSERT(manager.conjunction(x,not_x).is_false())
        ARIADNE_TEST_ASSERT(manager.disjunction(x,not_x).is_true())
        ARIADNE_TEST_ASSERT(manager.conjunction(x,y) == manager.conjunction(y,x))
        ARIADNE_TEST_ASSERT(manager.disjunction(x,y) == manager.disjunction(y,x))
    }

    void test_evaluation() {
        BddManager manager;
        auto x = manager.variable(0u);
        auto y = manager.variable(1u);
        auto formula = manager.exclusive_or(x,y);

        ARIADNE_TEST_EQUALS(manager.evaluate(formula,make_binary_word("00")),false)
        ARIADNE_TEST_EQUALS(manager.evaluate(formula,make_binary_word("01")),true)
        ARIADNE_TEST_EQUALS(manager.evaluate(formula,make_binary_word("10")),true)
        ARIADNE_TEST_EQUALS(manager.evaluate(formula,make_binary_word("11")),false)
        ARIADNE_TEST_FAIL(manager.evaluate(y,make_binary_word("0")))
    }

    void test_manager_separation() {
        BddManager first;
        BddManager second;
        auto x = first.variable(0u);
        auto y = second.variable(0u);

        ARIADNE_TEST_ASSERT(x != y)
        ARIADNE_TEST_FAIL(first.conjunction(x,y))
    }

    void test_concurrent_canonical_construction() {
        auto& thread_manager = ThreadManager::instance();
        SizeType maximum = thread_manager.maximum_concurrency();
        if (maximum == 0u) return;

        SizeType previous = thread_manager.concurrency();
        thread_manager.set_concurrency(std::min<SizeType>(maximum,4u));

        BddManager manager;
        auto x = manager.variable(0u);
        auto y = manager.variable(1u);
        auto not_y = manager.negation(y);

        std::vector<std::future<Bdd>> futures;
        for (SizeType i = 0u; i != 32u; ++i) {
            futures.push_back(thread_manager.enqueue([&manager,x,y,not_y] {
                auto lhs = manager.conjunction(x,y);
                auto rhs = manager.conjunction(x,not_y);
                return manager.disjunction(lhs,rhs);
            }));
        }

        for (auto& future : futures) {
            ARIADNE_TEST_ASSERT(future.get() == x)
        }

        thread_manager.set_concurrency(previous);
    }

    void test() {
        ARIADNE_TEST_CALL(test_constants_and_variables())
        ARIADNE_TEST_CALL(test_boolean_identities())
        ARIADNE_TEST_CALL(test_evaluation())
        ARIADNE_TEST_CALL(test_manager_separation())
        ARIADNE_TEST_CALL(test_concurrent_canonical_construction())
    }
};

int main() {
    TestBdd().test();
    return ARIADNE_TEST_FAILURES;
}
