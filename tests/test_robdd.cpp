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
#include <map>
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
            manager.make_node(BddVariable(std::numeric_limits<SizeType>::max()),f,t));
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

    void test_support() {
        RobddManager manager;
        auto f = manager.constant(false);
        auto t = manager.constant(true);
        auto x = manager.variable(1u);
        auto z = manager.variable(5u);

        ARIADNE_TESTING_PROVE(manager.support(f).empty());
        ARIADNE_TESTING_PROVE(manager.support(t).empty());
        auto function = manager.ite(x,z,manager.negation(z));
        auto variables = manager.support(function);
        ARIADNE_TESTING_EXPECT_EQUAL(variables.size(),2u);
        ARIADNE_TESTING_EXPECT_EQUAL(variables.at(0u).index(),1u);
        ARIADNE_TESTING_EXPECT_EQUAL(variables.at(1u).index(),5u);
        auto opposite = manager.support(manager.negation(function));
        ARIADNE_TESTING_PROVE(variables == opposite);
        ARIADNE_TESTING_EXPECT_EQUAL(
            manager.support(manager.make_node(BddVariable(0u),z,z)).size(),1u);

        RobddManager foreign;
        ARIADNE_TESTING_EXPECT_EXCEPTION(manager.support(foreign.variable(1u)));
    }

    void test_shift() {
        RobddManager manager;
        auto f = manager.constant(false);
        auto x = manager.variable(1u);
        auto z = manager.variable(4u);
        auto formula = manager.conjunction(x,manager.negation(z));

        ARIADNE_TESTING_PROVE(manager.shift(f,13) == f);
        ARIADNE_TESTING_PROVE(manager.shift(manager.constant(true),-13).is_true());
        SizeType before = manager.node_count();
        ARIADNE_TESTING_PROVE(manager.shift(formula,0) == formula);
        ARIADNE_TESTING_EXPECT_EQUAL(manager.node_count(),before);

        auto moved_up = manager.shift(formula,2);
        auto expected_up = manager.conjunction(
            manager.variable(3u),manager.negation(manager.variable(6u)));
        ARIADNE_TESTING_PROVE(moved_up == expected_up);
        auto moved_down = manager.shift(formula,-1);
        auto expected_down = manager.conjunction(
            manager.variable(0u),manager.negation(manager.variable(3u)));
        ARIADNE_TESTING_PROVE(moved_down == expected_down);
        ARIADNE_TESTING_PROVE(
            manager.shift(manager.negation(formula),2) == manager.negation(moved_up));
        ARIADNE_TESTING_PROVE(manager.shift(moved_up,-2) == formula);

        // Both branches refer to one physical child with opposite polarity.
        // Shifting the shared DAG must reuse the memoized transformed child.
        auto shared_child = manager.variable(7u);
        auto shared_root = manager.ite(
            manager.variable(0u),shared_child,manager.negation(shared_child));
        ARIADNE_TESTING_PROVE(
            manager.low(shared_root) == manager.negation(manager.high(shared_root)));
        auto shifted_shared = manager.shift(shared_root,2);
        auto expected_shared = manager.ite(
            manager.variable(2u),manager.variable(9u),
            manager.negation(manager.variable(9u)));
        ARIADNE_TESTING_PROVE(shifted_shared == expected_shared);
        ARIADNE_TESTING_PROVE(
            manager.shift(manager.negation(shared_root),2)
            == manager.negation(shifted_shared));

        ARIADNE_TESTING_EXPECT_EXCEPTION(manager.shift(formula,-2));
        ARIADNE_TESTING_EXPECT_EXCEPTION(
            manager.shift(formula,std::numeric_limits<std::ptrdiff_t>::min()));
        SizeType max_index = std::numeric_limits<SizeType>::max();
        auto near_max = manager.variable(max_index-2u);
        ARIADNE_TESTING_EXPECT_EXCEPTION(manager.shift(near_max,3));

        RobddManager foreign;
        ARIADNE_TESTING_EXPECT_EXCEPTION(manager.shift(foreign.variable(1u),1));
    }

    void test_rename() {
        RobddManager manager;
        auto x = manager.variable(0u);
        auto y = manager.variable(2u);
        auto formula = manager.conjunction(x,manager.negation(y));

        ARIADNE_TESTING_PROVE(manager.rename(formula,{}) == formula);
        ARIADNE_TESTING_PROVE(manager.rename(manager.constant(true),{{0u,3u}}).is_true());
        ARIADNE_TESTING_PROVE(manager.rename(formula,{{0u,0u},{2u,2u}}) == formula);
        auto swapped = manager.rename(formula,{{0u,2u},{2u,0u}});
        ARIADNE_TESTING_PROVE(swapped == manager.conjunction(y,manager.negation(x)));
        auto reordered = manager.rename(formula,{{0u,3u},{2u,1u}});
        ARIADNE_TESTING_PROVE(reordered == manager.conjunction(
            manager.variable(3u),manager.negation(manager.variable(1u))));

        ARIADNE_TESTING_PROVE(manager.rename(formula,{{0u,2u}}).is_false());
        ARIADNE_TESTING_PROVE(manager.rename(formula,{{0u,3u},{2u,3u}}).is_false());
        auto complement = manager.negation(formula);
        ARIADNE_TESTING_PROVE(
            manager.rename(complement,{{0u,2u},{2u,0u}}) == manager.negation(swapped));
        ARIADNE_TESTING_PROVE(manager.rename(formula,{{5u,6u}}) == formula);

        SizeType sentinel = std::numeric_limits<SizeType>::max();
        ARIADNE_TESTING_EXPECT_EXCEPTION(manager.rename(formula,{{0u,sentinel}}));
        ARIADNE_TESTING_EXPECT_EXCEPTION(manager.rename(formula,{{sentinel,0u}}));

        RobddManager foreign;
        ARIADNE_TESTING_EXPECT_EXCEPTION(manager.rename(foreign.variable(0u),{}));
    }

    void test_rename_truth_tables() {
        RobddManager manager;
        std::array<Robdd,3> variables {
            manager.variable(0u),manager.variable(1u),manager.variable(2u)
        };
        std::array<std::map<SizeType,SizeType>,5> mappings {{
            {{0u,1u},{1u,0u}},
            {{0u,2u},{2u,0u}},
            {{0u,3u},{1u,2u},{2u,1u}},
            {{0u,1u},{1u,1u}},
            {{1u,3u},{2u,3u}}
        }};
        char const* const assignments[16] = {
            "0000","1000","0100","1100","0010","1010","0110","1110",
            "0001","1001","0101","1101","0011","1011","0111","1111"
        };
        for (unsigned mask=0u; mask!=256u; ++mask) {
            Robdd source = manager.constant(false);
            for (unsigned combination=0u; combination!=8u; ++combination) {
                if (((mask >> combination) & 1u) == 0u) continue;
                Robdd cube = manager.constant(true);
                for (SizeType index=0u; index!=3u; ++index) {
                    Robdd literal = ((combination >> index) & 1u)
                        ? variables[index] : manager.negation(variables[index]);
                    cube = manager.conjunction(cube,literal);
                }
                source = manager.disjunction(source,cube);
            }
            for (auto const& mapping : mappings) {
                Robdd renamed = manager.rename(source,mapping);
                for (unsigned assignment=0u; assignment!=16u; ++assignment) {
                    unsigned original_assignment = 0u;
                    for (SizeType index=0u; index!=3u; ++index) {
                        auto found = mapping.find(index);
                        SizeType destination = found == mapping.end() ? index : found->second;
                        if (((assignment >> destination) & 1u) != 0u) {
                            original_assignment |= (1u << index);
                        }
                    }
                    Bool expected = ((mask >> original_assignment) & 1u) != 0u;
                    ARIADNE_TESTING_EXPECT_EQUAL(
                        manager.evaluate(renamed,make_binary_word(assignments[assignment])),
                        expected);
                }
            }
        }
    }

    void test_restrict_and_quantification_basics() {
        RobddManager manager;
        auto f = manager.constant(false);
        auto t = manager.constant(true);
        auto x = manager.variable(0u);
        auto y = manager.variable(2u);
        auto z = manager.variable(4u);
        auto formula = manager.conjunction(x,manager.exclusive_or(y,z));

        ARIADNE_TESTING_PROVE(manager.restrict(f,BddVariable(2u),true) == f);
        ARIADNE_TESTING_PROVE(manager.restrict(t,BddVariable(2u),false) == t);
        ARIADNE_TESTING_PROVE(manager.restrict(formula,BddVariable(5u),true) == formula);
        ARIADNE_TESTING_PROVE(manager.restrict(formula,BddVariable(1u),false) == formula);
        ARIADNE_TESTING_PROVE(manager.restrict(formula,BddVariable(0u),false).is_false());
        ARIADNE_TESTING_PROVE(
            manager.restrict(formula,BddVariable(2u),true)
            == manager.conjunction(x,manager.negation(z)));
        ARIADNE_TESTING_PROVE(
            manager.restrict(formula,BddVariable(4u),true)
            == manager.conjunction(x,manager.negation(y)));
        ARIADNE_TESTING_PROVE(
            manager.restrict(manager.negation(formula),BddVariable(2u),true)
            == manager.negation(manager.restrict(formula,BddVariable(2u),true)));

        ARIADNE_TESTING_PROVE(manager.exists(f,{BddVariable(2u)}) == f);
        ARIADNE_TESTING_PROVE(manager.exists(t,{BddVariable(2u)}) == t);
        ARIADNE_TESTING_PROVE(manager.exists(formula,{}) == formula);
        ARIADNE_TESTING_PROVE(manager.exists(formula,{BddVariable(1u)}) == formula);
        ARIADNE_TESTING_PROVE(manager.exists(formula,
            {BddVariable(2u),BddVariable(2u)}) == x);
        ARIADNE_TESTING_PROVE(manager.exists(formula,
            {BddVariable(2u),BddVariable(4u)}) == x);
        ARIADNE_TESTING_PROVE(manager.exists(formula,
            {BddVariable(0u),BddVariable(2u),BddVariable(4u)}).is_true());

        // Existential quantification does NOT commute with negation.
        ARIADNE_TESTING_PROVE(
            manager.exists(x,{BddVariable(0u)}).is_true());
        ARIADNE_TESTING_PROVE(
            manager.exists(manager.negation(x),{BddVariable(0u)}).is_true());
        ARIADNE_TESTING_PROVE(
            manager.and_exists(x,manager.negation(x),{BddVariable(0u)}).is_false());
        ARIADNE_TESTING_PROVE(
            manager.and_exists(x,t,{BddVariable(0u)}).is_true());
        ARIADNE_TESTING_PROVE(
            manager.and_exists(t,x,{BddVariable(0u)}).is_true());
        ARIADNE_TESTING_PROVE(
            manager.and_exists(formula,t,{BddVariable(2u)}) == x);
        ARIADNE_TESTING_PROVE(
            manager.and_exists(formula,manager.negation(y),{BddVariable(2u)})
            == manager.conjunction(x,z));
        ARIADNE_TESTING_PROVE(
            manager.and_exists(formula,t,{}) == formula);
        ARIADNE_TESTING_PROVE(
            manager.and_exists(formula,formula,{BddVariable(2u)}) == x);
        ARIADNE_TESTING_PROVE(
            manager.and_exists(formula,manager.negation(formula),
                {BddVariable(0u),BddVariable(2u)}).is_false());
        ARIADNE_TESTING_PROVE(
            manager.and_exists(formula,f,{BddVariable(2u)}).is_false());
        ARIADNE_TESTING_PROVE(
            manager.and_exists(formula,formula,{BddVariable(1u)}) == formula);

        SizeType sentinel = std::numeric_limits<SizeType>::max();
        ARIADNE_TESTING_EXPECT_EXCEPTION(
            manager.restrict(formula,BddVariable(sentinel),false));
        ARIADNE_TESTING_EXPECT_EXCEPTION(
            manager.exists(formula,{BddVariable(sentinel)}));
        ARIADNE_TESTING_EXPECT_EXCEPTION(
            manager.and_exists(formula,t,{BddVariable(sentinel)}));

        RobddManager other;
        auto foreign = other.variable(0u);
        ARIADNE_TESTING_EXPECT_EXCEPTION(
            manager.restrict(foreign,BddVariable(0u),true));
        ARIADNE_TESTING_EXPECT_EXCEPTION(
            manager.exists(foreign,{BddVariable(0u)}));
        ARIADNE_TESTING_EXPECT_EXCEPTION(
            manager.and_exists(formula,foreign,{BddVariable(0u)}));
    }

    void test_quantification_truth_tables() {
        RobddManager manager;
        std::array<Robdd,3> variables {
            manager.variable(0u),manager.variable(1u),manager.variable(2u)
        };
        char const* const assignments[8] = {
            "000","100","010","110","001","101","011","111"
        };

        std::vector<Robdd> functions;
        functions.reserve(256u);
        for (unsigned mask=0u; mask!=256u; ++mask) {
            Robdd formula = manager.constant(false);
            for (unsigned pattern=0u; pattern!=8u; ++pattern) {
                if (((mask >> pattern) & 1u) == 0u) continue;
                Robdd cube = manager.constant(true);
                for (SizeType index=0u; index!=3u; ++index) {
                    Robdd literal = ((pattern >> index) & 1u)
                        ? variables[index] : manager.negation(variables[index]);
                    cube = manager.conjunction(cube,literal);
                }
                formula = manager.disjunction(formula,cube);
            }
            functions.push_back(formula);
        }

        for (unsigned mask=0u; mask!=256u; ++mask) {
            Robdd formula = functions[mask];

            for (unsigned variable=0u; variable!=3u; ++variable) {
                for (unsigned chosen=0u; chosen!=2u; ++chosen) {
                    Robdd restricted = manager.restrict(
                        formula,BddVariable(variable),chosen != 0u);
                    for (unsigned assignment=0u; assignment!=8u; ++assignment) {
                        unsigned forced = (assignment & ~(1u << variable))
                            | (chosen << variable);
                        ARIADNE_TESTING_EXPECT_EQUAL(
                            manager.evaluate(restricted,
                                make_binary_word(assignments[assignment])),
                            ((mask >> forced) & 1u) != 0u);
                    }
                }
            }

            for (unsigned quant_mask=0u; quant_mask!=8u; ++quant_mask) {
                std::vector<BddVariable> quantified;
                for (unsigned index=0u; index!=3u; ++index) {
                    if ((quant_mask & (1u << index)) != 0u) {
                        quantified.emplace_back(index);
                    }
                }
                Robdd projected = manager.exists(formula,quantified);
                unsigned other_mask = (mask * 73u + 29u) & 255u;
                Robdd fused = manager.and_exists(
                    formula,functions[other_mask],quantified);
                Robdd reference = manager.exists(
                    manager.conjunction(formula,functions[other_mask]),quantified);
                ARIADNE_TESTING_PROVE(fused == reference);
                ARIADNE_TESTING_PROVE(
                    manager.and_exists(functions[other_mask],formula,quantified) == fused);

                for (unsigned assignment=0u; assignment!=8u; ++assignment) {
                    Bool expected_projected = false;
                    Bool expected_fused = false;
                    for (unsigned candidate=0u; candidate!=8u; ++candidate) {
                        if (((candidate ^ assignment) & ~quant_mask) != 0u) continue;
                        expected_projected = expected_projected
                            or (((mask >> candidate) & 1u) != 0u);
                        expected_fused = expected_fused
                            or (((mask >> candidate) & 1u) != 0u
                                and ((other_mask >> candidate) & 1u) != 0u);
                    }
                    ARIADNE_TESTING_EXPECT_EQUAL(
                        manager.evaluate(projected,make_binary_word(assignments[assignment])),
                        expected_projected);
                    ARIADNE_TESTING_EXPECT_EQUAL(
                        manager.evaluate(fused,make_binary_word(assignments[assignment])),
                        expected_fused);
                }
            }
        }
    }

    void test_models_finite_universe() {
        RobddManager manager;
        auto f = manager.constant(false);
        auto t = manager.constant(true);
        auto x0 = manager.variable(0u);
        auto x2 = manager.variable(2u);

        ARIADNE_TESTING_EXPECT_EQUAL(manager.model_count(f,0u),0u);
        ARIADNE_TESTING_EXPECT_EQUAL(manager.model_count(t,0u),1u);
        ARIADNE_TESTING_EXPECT_EQUAL(manager.model_count(f,5u),0u);
        ARIADNE_TESTING_EXPECT_EQUAL(manager.model_count(t,5u),32u);

        std::vector<BinaryWord> empty_universe;
        manager.for_each_satisfying_assignment(t,0u,[&](BinaryWord const& word) {
            empty_universe.push_back(word);
        });
        ARIADNE_TESTING_EXPECT_EQUAL(empty_universe.size(),1u);
        ARIADNE_TESTING_PROVE(empty_universe.front().empty());

        std::vector<BinaryWord> no_assignments;
        manager.for_each_satisfying_assignment(f,3u,[&](BinaryWord const& word) {
            no_assignments.push_back(word);
        });
        ARIADNE_TESTING_PROVE(no_assignments.empty());

        auto skipped = manager.conjunction(x0,manager.negation(x2));
        ARIADNE_TESTING_EXPECT_EQUAL(manager.model_count(x2,3u),4u);
        ARIADNE_TESTING_EXPECT_EQUAL(manager.model_count(skipped,3u),2u);
        ARIADNE_TESTING_EXPECT_EQUAL(manager.model_count(skipped,5u),8u);
        ARIADNE_TESTING_EXPECT_EQUAL(
            manager.model_count(manager.negation(skipped),3u),6u);

        std::vector<BinaryWord> assignments;
        manager.for_each_satisfying_assignment(skipped,3u,[&](BinaryWord const& word) {
            assignments.push_back(word);
        });
        ARIADNE_TESTING_EXPECT_EQUAL(assignments.size(),2u);
        ARIADNE_TESTING_PROVE(assignments[0u] == make_binary_word("100"));
        ARIADNE_TESTING_PROVE(assignments[1u] == make_binary_word("110"));
        SizeType before = manager.node_count();
        manager.for_each_satisfying_assignment(manager.negation(skipped),3u,
            [&](BinaryWord const& word) {
                ARIADNE_TESTING_PROVE(manager.evaluate(
                    manager.negation(skipped),word));
            });
        ARIADNE_TESTING_EXPECT_EQUAL(manager.node_count(),before);

        auto shifted = manager.shift(skipped,4);
        ARIADNE_TESTING_EXPECT_EQUAL(
            manager.model_count(shifted,3u,4u),2u);
        std::vector<BinaryWord> offset_assignments;
        manager.for_each_satisfying_assignment(shifted,3u,[&](BinaryWord const& word) {
            offset_assignments.push_back(word);
        },4u);
        ARIADNE_TESTING_PROVE(offset_assignments == assignments);

        ARIADNE_TESTING_EXPECT_EXCEPTION(manager.model_count(x2,2u));
        ARIADNE_TESTING_EXPECT_EXCEPTION(manager.model_count(x2,0u));
        ARIADNE_TESTING_EXPECT_EXCEPTION(manager.model_count(x0,2u,1u));
        ARIADNE_TESTING_EXPECT_EXCEPTION(manager.for_each_satisfying_assignment(
            x2,2u,[](BinaryWord const&) {}));
        ARIADNE_TESTING_EXPECT_EXCEPTION(manager.for_each_satisfying_assignment(
            x0,1u,std::function<void(BinaryWord const&)>{}));

        SizeType max_index = std::numeric_limits<SizeType>::max();
        SizeType bits = std::numeric_limits<SizeType>::digits;
        ARIADNE_TESTING_EXPECT_EXCEPTION(manager.model_count(t,bits));
        ARIADNE_TESTING_EXPECT_EQUAL(manager.model_count(f,bits),0u);
        ARIADNE_TESTING_EXPECT_EXCEPTION(manager.model_count(t,2u,max_index-1u));
        ARIADNE_TESTING_EXPECT_EXCEPTION(manager.model_count(t,0u,max_index));
        ARIADNE_TESTING_EXPECT_EXCEPTION(manager.for_each_satisfying_assignment(
            f,2u,[](BinaryWord const&) {},max_index-1u));

        auto near_limit = manager.variable(bits-1u);
        ARIADNE_TESTING_EXPECT_EQUAL(
            manager.model_count(near_limit,bits),SizeType(1u) << (bits-1u));
        auto singleton = manager.constant(true);
        for (SizeType i=0u; i!=bits; ++i) {
            singleton = manager.conjunction(singleton,manager.variable(i));
        }
        ARIADNE_TESTING_EXPECT_EQUAL(manager.model_count(singleton,bits),1u);
        // The complement has every assignment except the all-true word,
        // reaching the largest exactly representable model count.
        auto almost_all = manager.negation(singleton);
        ARIADNE_TESTING_EXPECT_EQUAL(manager.model_count(almost_all,bits),max_index);

        // Each cofactor of XOR has 2^(bits-1) satisfying assignments over
        // bits+1 variables: the individual counts fit, but their sum does not.
        auto xor_first_two = manager.exclusive_or(
            manager.variable(0u),manager.variable(1u));
        ARIADNE_TESTING_EXPECT_EQUAL(
            manager.model_count(xor_first_two,bits),SizeType(1u) << (bits-1u));
        ARIADNE_TESTING_EXPECT_EXCEPTION(
            manager.model_count(xor_first_two,bits+1u));

        RobddManager foreign;
        auto foreign_value = foreign.constant(true);
        ARIADNE_TESTING_EXPECT_EXCEPTION(manager.model_count(foreign_value,0u));
        ARIADNE_TESTING_EXPECT_EXCEPTION(manager.for_each_satisfying_assignment(
            foreign_value,0u,[](BinaryWord const&) {}));
    }

    void test_model_enumeration_truth_tables() {
        RobddManager manager;
        std::array<Robdd,3> variables {
            manager.variable(0u),manager.variable(1u),manager.variable(2u)
        };
        for (unsigned mask=0u; mask!=256u; ++mask) {
            Robdd formula = manager.constant(false);
            for (unsigned assignment=0u; assignment!=8u; ++assignment) {
                if (((mask >> assignment) & 1u) == 0u) continue;
                Robdd cube = manager.constant(true);
                for (SizeType variable=0u; variable!=3u; ++variable) {
                    Robdd literal = ((assignment >> variable) & 1u)
                        ? variables[variable] : manager.negation(variables[variable]);
                    cube = manager.conjunction(cube,literal);
                }
                formula = manager.disjunction(formula,cube);
            }

            std::vector<unsigned> observed;
            manager.for_each_satisfying_assignment(formula,3u,[&](BinaryWord const& word) {
                ARIADNE_TESTING_EXPECT_EQUAL(word.size(),3u);
                unsigned assignment = 0u;
                for (unsigned variable=0u; variable!=3u; ++variable) {
                    if (word[variable]) {
                        assignment |= (1u << variable);
                    }
                }
                observed.push_back(assignment);
            });
            std::sort(observed.begin(),observed.end());
            std::vector<unsigned> expected;
            for (unsigned assignment=0u; assignment!=8u; ++assignment) {
                if (((mask >> assignment) & 1u) != 0u) {
                    expected.push_back(assignment);
                }
            }
            ARIADNE_TESTING_PROVE(observed == expected);
            ARIADNE_TESTING_EXPECT_EQUAL(
                manager.model_count(formula,3u),expected.size());

            auto negated = manager.negation(formula);
            ARIADNE_TESTING_EXPECT_EQUAL(
                manager.model_count(negated,3u),8u-expected.size());
            ARIADNE_TESTING_EXPECT_EQUAL(
                manager.model_count(formula,4u),2u*expected.size());
        }
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
    ARIADNE_TESTING_ADD_TEST(TestRobdd, test_support);
    ARIADNE_TESTING_ADD_TEST(TestRobdd, test_shift);
    ARIADNE_TESTING_ADD_TEST(TestRobdd, test_rename);
    ARIADNE_TESTING_ADD_TEST(TestRobdd, test_rename_truth_tables);
    ARIADNE_TESTING_ADD_TEST(TestRobdd, test_restrict_and_quantification_basics);
    ARIADNE_TESTING_ADD_TEST(TestRobdd, test_quantification_truth_tables);
    ARIADNE_TESTING_ADD_TEST(TestRobdd, test_models_finite_universe);
    ARIADNE_TESTING_ADD_TEST(TestRobdd, test_model_enumeration_truth_tables);
    ARIADNE_TESTING_ADD_TEST(TestRobdd, test_exhaustive_three_variable_truth_tables);
    ARIADNE_TESTING_ADD_TEST(TestRobdd, test_randomized_four_variable_formulas);
    ARIADNE_TESTING_ADD_TEST(TestRobdd, test_evaluation);
    ARIADNE_TESTING_ADD_TEST(TestRobdd, test_manager_separation);
    ARIADNE_TESTING_ADD_TEST(TestRobdd, test_concurrent_canonical_construction);
    return ARIADNE_TESTING_RUN_TESTS_WITH_ARGS(argc, argv);
}
