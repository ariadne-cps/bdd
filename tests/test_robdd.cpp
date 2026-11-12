/***************************************************************************
 *            tests/test_robdd.cpp
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

/** @file tests/test_robdd.cpp
 *  @brief Verify ROBDD semantics, concurrency and canonical representations.
 */

#include <algorithm>
#include <array>
#include <barrier>
#include <cstdint>
#include <future>
#include <limits>
#include <map>
#include <vector>

#include "bdd/robdd.hpp"
#include "logging/logging.hpp"
#include "threading/thread_manager.hpp"
#include "threading/threading_command_line_interface.hpp"

#include "ariadne-testing.hpp"

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
        ARIADNE_TESTING_EXPECT_EQUAL(manager.node_count(), 1u);

        BddVariable first(0u);
        BddVariable first_again(0u);
        BddVariable second(1u);
        ARIADNE_TESTING_PROVE(first == first_again);
        ARIADNE_TESTING_PROVE(first != second);
        ARIADNE_TESTING_PROVE(first < second);

        ARIADNE_TESTING_PROVE(manager.ite(x, t, f) == x);
    }

    void test_boolean_identities() {
        RobddManager manager;
        auto f = manager.constant(false);
        auto t = manager.constant(true);
        auto x = manager.variable(0u);
        auto y = manager.variable(1u);
        auto not_x = manager.negation(x);

        ARIADNE_TESTING_PROVE(manager.conjunction(x, t) == x);
        ARIADNE_TESTING_PROVE(manager.disjunction(x, f) == x);
        ARIADNE_TESTING_PROVE(manager.conjunction(x, not_x).is_false());
        ARIADNE_TESTING_PROVE(manager.disjunction(x, not_x).is_true());
        ARIADNE_TESTING_PROVE(manager.conjunction(x, y) == manager.conjunction(y, x));
        ARIADNE_TESTING_PROVE(manager.disjunction(x, y) == manager.disjunction(y, x));
    }

    void test_ite_complement_cases() {
        RobddManager manager;
        auto f = manager.constant(false);
        auto t = manager.constant(true);
        auto x = manager.variable(0u);
        auto y = manager.variable(1u);

        auto complement = manager.ite(x, f, t);
        ARIADNE_TESTING_PROVE(complement == manager.negation(x));

        auto false_then = manager.ite(x, f, y);
        auto expected = manager.conjunction(manager.negation(x), y);
        ARIADNE_TESTING_PROVE(false_then == expected);
    }

    void test_complemented_edge_sharing() {
        RobddManager manager;
        auto x = manager.variable(0u);
        auto y = manager.variable(1u);
        auto formula = manager.conjunction(x, y);
        SizeType nodes_before_negation = manager.node_count();

        auto complement = manager.negation(formula);

        ARIADNE_TESTING_EXPECT_EQUAL(manager.node_count(), nodes_before_negation);
        ARIADNE_TESTING_PROVE(manager.negation(complement) == formula);

        auto not_x = manager.negation(x);
        auto not_y = manager.negation(y);
        SizeType nodes_before_equivalent_complement = manager.node_count();
        auto equivalent_complement = manager.disjunction(not_x, not_y);

        ARIADNE_TESTING_PROVE(equivalent_complement == complement);
        ARIADNE_TESTING_EXPECT_EQUAL(
            manager.node_count(), nodes_before_equivalent_complement);

        ARIADNE_TESTING_EXPECT_EQUAL(manager.evaluate(complement, make_binary_word("00")), true);
        ARIADNE_TESTING_EXPECT_EQUAL(manager.evaluate(complement, make_binary_word("01")), true);
        ARIADNE_TESTING_EXPECT_EQUAL(manager.evaluate(complement, make_binary_word("10")), true);
        ARIADNE_TESTING_EXPECT_EQUAL(manager.evaluate(complement, make_binary_word("11")), false);
    }

    void test_structural_inspection_and_reconstruction() {
        RobddManager manager;
        auto f = manager.constant(false);
        auto t = manager.constant(true);
        auto x = manager.variable(0u);
        auto z = manager.variable(3u);

        ARIADNE_TESTING_PROVE(not manager.top_variable(f).has_value());
        ARIADNE_TESTING_PROVE(not manager.top_variable(t).has_value());
        ARIADNE_TESTING_EXPECT_EQUAL(manager.top_variable(x)->index(), 0u);

        auto rooted = manager.make_node(BddVariable(0u), f, z);
        ARIADNE_TESTING_EXPECT_EQUAL(manager.top_variable(rooted)->index(), 0u);
        ARIADNE_TESTING_PROVE(manager.low(rooted) == f);
        ARIADNE_TESTING_PROVE(manager.high(rooted) == z);
        ARIADNE_TESTING_PROVE(manager.top_variable(manager.high(rooted))->index() == 3u);

        auto complement = manager.negation(rooted);
        SizeType count_before_inspection = manager.node_count();
        ARIADNE_TESTING_PROVE(manager.low(complement) == t);
        ARIADNE_TESTING_PROVE(manager.high(complement) == manager.negation(z));
        ARIADNE_TESTING_EXPECT_EQUAL(manager.node_count(), count_before_inspection);
        ARIADNE_TESTING_PROVE(
            manager.make_node(BddVariable(0u), manager.low(rooted), manager.high(rooted)) == rooted);
        ARIADNE_TESTING_PROVE(
            manager.make_node(BddVariable(0u), manager.low(complement), manager.high(complement)) == complement);

        ARIADNE_TESTING_PROVE(manager.make_node(BddVariable(0u), z, z) == z);
        ARIADNE_TESTING_PROVE(manager.make_node(BddVariable(0u), f, t) == x);
        ARIADNE_TESTING_PROVE(manager.make_node(BddVariable(0u), t, f) == manager.negation(x));

        ARIADNE_TESTING_EXPECT_EXCEPTION(manager.low(f));
        ARIADNE_TESTING_EXPECT_EXCEPTION(manager.high(t));
        ARIADNE_TESTING_EXPECT_EXCEPTION(manager.make_node(BddVariable(3u), f, z));
        ARIADNE_TESTING_EXPECT_EXCEPTION(manager.make_node(BddVariable(4u), z, f));
        ARIADNE_TESTING_EXPECT_EXCEPTION(manager.make_node(BddVariable(3u), z, z));
        ARIADNE_TESTING_EXPECT_EXCEPTION(
            manager.make_node(BddVariable(std::numeric_limits<SizeType>::max()), f, t));
        ARIADNE_TESTING_EXPECT_EXCEPTION(
            manager.variable(std::numeric_limits<SizeType>::max()));

        RobddManager other;
        auto other_value = other.variable(4u);
        ARIADNE_TESTING_EXPECT_EXCEPTION(manager.top_variable(other_value));
        ARIADNE_TESTING_EXPECT_EXCEPTION(manager.low(other_value));
        ARIADNE_TESTING_EXPECT_EXCEPTION(manager.high(other_value));
        ARIADNE_TESTING_EXPECT_EXCEPTION(
            manager.make_node(BddVariable(0u), f, other_value));
    }

    void test_support() {
        RobddManager manager;
        auto f = manager.constant(false);
        auto t = manager.constant(true);
        auto x = manager.variable(1u);
        auto z = manager.variable(5u);

        ARIADNE_TESTING_PROVE(manager.support(f).empty());
        ARIADNE_TESTING_PROVE(manager.support(t).empty());
        auto function = manager.ite(x, z, manager.negation(z));
        auto variables = manager.support(function);
        ARIADNE_TESTING_EXPECT_EQUAL(variables.size(), 2u);
        ARIADNE_TESTING_EXPECT_EQUAL(variables.at(0u).index(), 1u);
        ARIADNE_TESTING_EXPECT_EQUAL(variables.at(1u).index(), 5u);
        auto opposite = manager.support(manager.negation(function));
        ARIADNE_TESTING_PROVE(variables == opposite);
        ARIADNE_TESTING_EXPECT_EQUAL(
            manager.support(manager.make_node(BddVariable(0u), z, z)).size(), 1u);

        RobddManager foreign;
        ARIADNE_TESTING_EXPECT_EXCEPTION(manager.support(foreign.variable(1u)));
    }

    void test_shift() {
        RobddManager manager;
        auto f = manager.constant(false);
        auto x = manager.variable(1u);
        auto z = manager.variable(4u);
        auto formula = manager.conjunction(x, manager.negation(z));

        ARIADNE_TESTING_PROVE(manager.shift(f, 13) == f);
        ARIADNE_TESTING_PROVE(manager.shift(manager.constant(true), -13).is_true());
        SizeType before = manager.node_count();
        ARIADNE_TESTING_PROVE(manager.shift(formula, 0) == formula);
        ARIADNE_TESTING_EXPECT_EQUAL(manager.node_count(), before);

        auto moved_up = manager.shift(formula, 2);
        auto expected_up = manager.conjunction(
            manager.variable(3u), manager.negation(manager.variable(6u)));
        ARIADNE_TESTING_PROVE(moved_up == expected_up);
        auto moved_down = manager.shift(formula, -1);
        auto expected_down = manager.conjunction(
            manager.variable(0u), manager.negation(manager.variable(3u)));
        ARIADNE_TESTING_PROVE(moved_down == expected_down);
        ARIADNE_TESTING_PROVE(
            manager.shift(manager.negation(formula), 2) == manager.negation(moved_up));
        ARIADNE_TESTING_PROVE(manager.shift(moved_up, -2) == formula);

        // Both branches refer to one physical child with opposite polarity.
        // Shifting the shared DAG must reuse the memoized transformed child.
        auto shared_child = manager.variable(7u);
        auto shared_root = manager.ite(
            manager.variable(0u), shared_child, manager.negation(shared_child));
        ARIADNE_TESTING_PROVE(
            manager.low(shared_root) == manager.negation(manager.high(shared_root)));
        auto shifted_shared = manager.shift(shared_root, 2);
        auto expected_shared = manager.ite(
            manager.variable(2u), manager.variable(9u),
            manager.negation(manager.variable(9u)));
        ARIADNE_TESTING_PROVE(shifted_shared == expected_shared);
        ARIADNE_TESTING_PROVE(
            manager.shift(manager.negation(shared_root), 2) == manager.negation(shifted_shared));

        ARIADNE_TESTING_EXPECT_EXCEPTION(manager.shift(formula, -2));
        ARIADNE_TESTING_EXPECT_EXCEPTION(
            manager.shift(formula, std::numeric_limits<std::ptrdiff_t>::min()));
        SizeType max_index = std::numeric_limits<SizeType>::max();
        auto near_max = manager.variable(max_index - 2u);
        ARIADNE_TESTING_EXPECT_EXCEPTION(manager.shift(near_max, 3));

        RobddManager foreign;
        ARIADNE_TESTING_EXPECT_EXCEPTION(manager.shift(foreign.variable(1u), 1));
    }

    void test_rename() {
        RobddManager manager;
        auto x = manager.variable(0u);
        auto y = manager.variable(2u);
        auto formula = manager.conjunction(x, manager.negation(y));

        ARIADNE_TESTING_PROVE(manager.rename(formula, {}) == formula);
        ARIADNE_TESTING_PROVE(manager.rename(manager.constant(true), {
                                                                         {0u, 3u}
        })
                                  .is_true());
        ARIADNE_TESTING_PROVE(manager.rename(formula, {
                                                          {0u, 0u},
                                                          {2u, 2u}
        }) == formula);
        auto swapped = manager.rename(formula, {
                                                   {0u, 2u},
                                                   {2u, 0u}
        });
        ARIADNE_TESTING_PROVE(swapped == manager.conjunction(y, manager.negation(x)));
        auto reordered = manager.rename(formula, {
                                                     {0u, 3u},
                                                     {2u, 1u}
        });
        ARIADNE_TESTING_PROVE(reordered == manager.conjunction(
                                               manager.variable(3u), manager.negation(manager.variable(1u))));

        ARIADNE_TESTING_PROVE(manager.rename(formula, {
                                                          {0u, 2u}
        })
                                  .is_false());
        ARIADNE_TESTING_PROVE(manager.rename(formula, {
                                                          {0u, 3u},
                                                          {2u, 3u}
        })
                                  .is_false());
        auto complement = manager.negation(formula);
        ARIADNE_TESTING_PROVE(
            manager.rename(complement, {
                                           {0u, 2u},
                                           {2u, 0u}
        }) == manager.negation(swapped));
        ARIADNE_TESTING_PROVE(manager.rename(formula, {
                                                          {5u, 6u}
        }) == formula);

        SizeType sentinel = std::numeric_limits<SizeType>::max();
        ARIADNE_TESTING_EXPECT_EXCEPTION(manager.rename(formula, {
                                                                     {0u, sentinel}
        }));
        ARIADNE_TESTING_EXPECT_EXCEPTION(manager.rename(formula, {
                                                                     {sentinel, 0u}
        }));

        RobddManager foreign;
        ARIADNE_TESTING_EXPECT_EXCEPTION(manager.rename(foreign.variable(0u), {}));
    }

    void test_rename_truth_tables() {
        RobddManager manager;
        std::array<Robdd, 3> variables{
            manager.variable(0u), manager.variable(1u), manager.variable(2u)};
        std::array<std::map<SizeType, SizeType>, 5> mappings{
            {{{0u, 1u}, {1u, 0u}},
             {{0u, 2u}, {2u, 0u}},
             {{0u, 3u}, {1u, 2u}, {2u, 1u}},
             {{0u, 1u}, {1u, 1u}},
             {{1u, 3u}, {2u, 3u}}}
        };
        const char* const assignments[16] = {
            "0000", "1000", "0100", "1100", "0010", "1010", "0110", "1110",
            "0001", "1001", "0101", "1101", "0011", "1011", "0111", "1111"};
        for (unsigned mask = 0u; mask != 256u; ++mask) {
            Robdd source = manager.constant(false);
            for (unsigned combination = 0u; combination != 8u; ++combination) {
                if (((mask >> combination) & 1u) == 0u)
                    continue;
                Robdd cube = manager.constant(true);
                for (SizeType index = 0u; index != 3u; ++index) {
                    Robdd literal = ((combination >> index) & 1u)
                                      ? variables[index]
                                      : manager.negation(variables[index]);
                    cube = manager.conjunction(cube, literal);
                }
                source = manager.disjunction(source, cube);
            }
            for (const auto& mapping: mappings) {
                Robdd renamed = manager.rename(source, mapping);
                for (unsigned assignment = 0u; assignment != 16u; ++assignment) {
                    unsigned original_assignment = 0u;
                    for (SizeType index = 0u; index != 3u; ++index) {
                        auto found = mapping.find(index);
                        SizeType destination = found == mapping.end() ? index : found->second;
                        if (((assignment >> destination) & 1u) != 0u) {
                            original_assignment |= (1u << index);
                        }
                    }
                    Bool expected = ((mask >> original_assignment) & 1u) != 0u;
                    ARIADNE_TESTING_EXPECT_EQUAL(
                        manager.evaluate(renamed, make_binary_word(assignments[assignment])),
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
        auto formula = manager.conjunction(x, manager.exclusive_or(y, z));

        ARIADNE_TESTING_PROVE(manager.restrict(f, BddVariable(2u), true) == f);
        ARIADNE_TESTING_PROVE(manager.restrict(t, BddVariable(2u), false) == t);
        ARIADNE_TESTING_PROVE(manager.restrict(formula, BddVariable(5u), true) == formula);
        ARIADNE_TESTING_PROVE(manager.restrict(formula, BddVariable(1u), false) == formula);
        ARIADNE_TESTING_PROVE(manager.restrict(formula, BddVariable(0u), false).is_false());
        ARIADNE_TESTING_PROVE(
            manager.restrict(formula, BddVariable(2u), true) == manager.conjunction(x, manager.negation(z)));
        ARIADNE_TESTING_PROVE(
            manager.restrict(formula, BddVariable(4u), true) == manager.conjunction(x, manager.negation(y)));
        ARIADNE_TESTING_PROVE(
            manager.restrict(manager.negation(formula), BddVariable(2u), true) == manager.negation(manager.restrict(formula, BddVariable(2u), true)));

        ARIADNE_TESTING_PROVE(manager.exists(f, {BddVariable(2u)}) == f);
        ARIADNE_TESTING_PROVE(manager.exists(t, {BddVariable(2u)}) == t);
        ARIADNE_TESTING_PROVE(manager.exists(formula, {}) == formula);
        ARIADNE_TESTING_PROVE(manager.exists(formula, {BddVariable(1u)}) == formula);
        ARIADNE_TESTING_PROVE(manager.exists(formula,
                                             {BddVariable(2u), BddVariable(2u)}) == x);
        ARIADNE_TESTING_PROVE(manager.exists(formula,
                                             {BddVariable(2u), BddVariable(4u)}) == x);
        ARIADNE_TESTING_PROVE(manager.exists(formula,
                                             {BddVariable(0u), BddVariable(2u), BddVariable(4u)})
                                  .is_true());

        // Existential quantification does NOT commute with negation.
        ARIADNE_TESTING_PROVE(
            manager.exists(x, {BddVariable(0u)}).is_true());
        ARIADNE_TESTING_PROVE(
            manager.exists(manager.negation(x), {BddVariable(0u)}).is_true());
        ARIADNE_TESTING_PROVE(
            manager.and_exists(x, manager.negation(x), {BddVariable(0u)}).is_false());
        ARIADNE_TESTING_PROVE(
            manager.and_exists(x, t, {BddVariable(0u)}).is_true());
        ARIADNE_TESTING_PROVE(
            manager.and_exists(t, x, {BddVariable(0u)}).is_true());
        ARIADNE_TESTING_PROVE(
            manager.and_exists(formula, t, {BddVariable(2u)}) == x);
        ARIADNE_TESTING_PROVE(
            manager.and_exists(formula, manager.negation(y), {BddVariable(2u)}) == manager.conjunction(x, z));
        ARIADNE_TESTING_PROVE(
            manager.and_exists(formula, t, {}) == formula);
        ARIADNE_TESTING_PROVE(
            manager.and_exists(formula, formula, {BddVariable(2u)}) == x);
        ARIADNE_TESTING_PROVE(
            manager.and_exists(formula, manager.negation(formula),
                               {BddVariable(0u), BddVariable(2u)})
                .is_false());
        ARIADNE_TESTING_PROVE(
            manager.and_exists(formula, f, {BddVariable(2u)}).is_false());
        ARIADNE_TESTING_PROVE(
            manager.and_exists(formula, formula, {BddVariable(1u)}) == formula);

        SizeType sentinel = std::numeric_limits<SizeType>::max();
        ARIADNE_TESTING_EXPECT_EXCEPTION(
            manager.restrict(formula, BddVariable(sentinel), false));
        ARIADNE_TESTING_EXPECT_EXCEPTION(
            manager.exists(formula, {BddVariable(sentinel)}));
        ARIADNE_TESTING_EXPECT_EXCEPTION(
            manager.and_exists(formula, t, {BddVariable(sentinel)}));

        RobddManager other;
        auto foreign = other.variable(0u);
        ARIADNE_TESTING_EXPECT_EXCEPTION(
            manager.restrict(foreign, BddVariable(0u), true));
        ARIADNE_TESTING_EXPECT_EXCEPTION(
            manager.exists(foreign, {BddVariable(0u)}));
        ARIADNE_TESTING_EXPECT_EXCEPTION(
            manager.and_exists(formula, foreign, {BddVariable(0u)}));
    }

    void test_quantification_truth_tables() {
        RobddManager manager;
        std::array<Robdd, 3> variables{
            manager.variable(0u), manager.variable(1u), manager.variable(2u)};
        const char* const assignments[8] = {
            "000", "100", "010", "110", "001", "101", "011", "111"};

        std::vector<Robdd> functions;
        functions.reserve(256u);
        for (unsigned mask = 0u; mask != 256u; ++mask) {
            Robdd formula = manager.constant(false);
            for (unsigned pattern = 0u; pattern != 8u; ++pattern) {
                if (((mask >> pattern) & 1u) == 0u)
                    continue;
                Robdd cube = manager.constant(true);
                for (SizeType index = 0u; index != 3u; ++index) {
                    Robdd literal = ((pattern >> index) & 1u)
                                      ? variables[index]
                                      : manager.negation(variables[index]);
                    cube = manager.conjunction(cube, literal);
                }
                formula = manager.disjunction(formula, cube);
            }
            functions.push_back(formula);
        }

        for (unsigned mask = 0u; mask != 256u; ++mask) {
            Robdd formula = functions[mask];

            for (unsigned variable = 0u; variable != 3u; ++variable) {
                for (unsigned chosen = 0u; chosen != 2u; ++chosen) {
                    Robdd restricted = manager.restrict(
                        formula, BddVariable(variable), chosen != 0u);
                    for (unsigned assignment = 0u; assignment != 8u; ++assignment) {
                        unsigned forced = (assignment & ~(1u << variable)) | (chosen << variable);
                        ARIADNE_TESTING_EXPECT_EQUAL(
                            manager.evaluate(restricted,
                                             make_binary_word(assignments[assignment])),
                            ((mask >> forced) & 1u) != 0u);
                    }
                }
            }

            for (unsigned quant_mask = 0u; quant_mask != 8u; ++quant_mask) {
                std::vector<BddVariable> quantified;
                for (unsigned index = 0u; index != 3u; ++index) {
                    if ((quant_mask & (1u << index)) != 0u) {
                        quantified.emplace_back(index);
                    }
                }
                Robdd projected = manager.exists(formula, quantified);
                unsigned other_mask = (mask * 73u + 29u) & 255u;
                Robdd fused = manager.and_exists(
                    formula, functions[other_mask], quantified);
                Robdd reference = manager.exists(
                    manager.conjunction(formula, functions[other_mask]), quantified);
                ARIADNE_TESTING_PROVE(fused == reference);
                ARIADNE_TESTING_PROVE(
                    manager.and_exists(functions[other_mask], formula, quantified) == fused);

                for (unsigned assignment = 0u; assignment != 8u; ++assignment) {
                    Bool expected_projected = false;
                    Bool expected_fused = false;
                    for (unsigned candidate = 0u; candidate != 8u; ++candidate) {
                        if (((candidate ^ assignment) & ~quant_mask) != 0u)
                            continue;
                        expected_projected = expected_projected or (((mask >> candidate) & 1u) != 0u);
                        expected_fused = expected_fused or (((mask >> candidate) & 1u) != 0u and ((other_mask >> candidate) & 1u) != 0u);
                    }
                    ARIADNE_TESTING_EXPECT_EQUAL(
                        manager.evaluate(projected, make_binary_word(assignments[assignment])),
                        expected_projected);
                    ARIADNE_TESTING_EXPECT_EQUAL(
                        manager.evaluate(fused, make_binary_word(assignments[assignment])),
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

        ARIADNE_TESTING_EXPECT_EQUAL(manager.model_count(f, 0u), 0u);
        ARIADNE_TESTING_EXPECT_EQUAL(manager.model_count(t, 0u), 1u);
        ARIADNE_TESTING_EXPECT_EQUAL(manager.model_count(f, 5u), 0u);
        ARIADNE_TESTING_EXPECT_EQUAL(manager.model_count(t, 5u), 32u);

        std::vector<BinaryWord> empty_universe;
        manager.for_each_satisfying_assignment(t, 0u, [&](const BinaryWord& word) {
            empty_universe.push_back(word);
        });
        ARIADNE_TESTING_EXPECT_EQUAL(empty_universe.size(), 1u);
        ARIADNE_TESTING_PROVE(empty_universe.front().empty());

        std::vector<BinaryWord> no_assignments;
        manager.for_each_satisfying_assignment(f, 3u, [&](const BinaryWord& word) {
            no_assignments.push_back(word);
        });
        ARIADNE_TESTING_PROVE(no_assignments.empty());

        auto skipped = manager.conjunction(x0, manager.negation(x2));
        ARIADNE_TESTING_EXPECT_EQUAL(manager.model_count(x2, 3u), 4u);
        ARIADNE_TESTING_EXPECT_EQUAL(manager.model_count(skipped, 3u), 2u);
        ARIADNE_TESTING_EXPECT_EQUAL(manager.model_count(skipped, 5u), 8u);
        ARIADNE_TESTING_EXPECT_EQUAL(
            manager.model_count(manager.negation(skipped), 3u), 6u);

        std::vector<BinaryWord> assignments;
        manager.for_each_satisfying_assignment(skipped, 3u, [&](const BinaryWord& word) {
            assignments.push_back(word);
        });
        ARIADNE_TESTING_EXPECT_EQUAL(assignments.size(), 2u);
        ARIADNE_TESTING_PROVE(assignments[0u] == make_binary_word("100"));
        ARIADNE_TESTING_PROVE(assignments[1u] == make_binary_word("110"));
        SizeType before = manager.node_count();
        manager.for_each_satisfying_assignment(manager.negation(skipped), 3u,
                                               [&](const BinaryWord& word) {
                                                   ARIADNE_TESTING_PROVE(manager.evaluate(
                                                       manager.negation(skipped), word));
                                               });
        ARIADNE_TESTING_EXPECT_EQUAL(manager.node_count(), before);

        auto shifted = manager.shift(skipped, 4);
        ARIADNE_TESTING_EXPECT_EQUAL(
            manager.model_count(shifted, 3u, 4u), 2u);
        std::vector<BinaryWord> offset_assignments;
        manager.for_each_satisfying_assignment(shifted, 3u, [&](const BinaryWord& word) { offset_assignments.push_back(word); }, 4u);
        ARIADNE_TESTING_PROVE(offset_assignments == assignments);

        ARIADNE_TESTING_EXPECT_EXCEPTION(manager.model_count(x2, 2u));
        ARIADNE_TESTING_EXPECT_EXCEPTION(manager.model_count(x2, 0u));
        ARIADNE_TESTING_EXPECT_EXCEPTION(manager.model_count(x0, 2u, 1u));
        ARIADNE_TESTING_EXPECT_EXCEPTION(manager.for_each_satisfying_assignment(
            x2, 2u, [](const BinaryWord&) {}));
        ARIADNE_TESTING_EXPECT_EXCEPTION(manager.for_each_satisfying_assignment(
            x0, 1u, std::function<void(const BinaryWord&)>{}));

        SizeType max_index = std::numeric_limits<SizeType>::max();
        SizeType bits = std::numeric_limits<SizeType>::digits;
        ARIADNE_TESTING_EXPECT_EXCEPTION(manager.model_count(t, bits));
        ARIADNE_TESTING_EXPECT_EQUAL(manager.model_count(f, bits), 0u);
        ARIADNE_TESTING_EXPECT_EXCEPTION(manager.model_count(t, 2u, max_index - 1u));
        ARIADNE_TESTING_EXPECT_EXCEPTION(manager.model_count(t, 0u, max_index));
        ARIADNE_TESTING_EXPECT_EXCEPTION(manager.for_each_satisfying_assignment(
            f, 2u, [](const BinaryWord&) {}, max_index - 1u));

        auto near_limit = manager.variable(bits - 1u);
        ARIADNE_TESTING_EXPECT_EQUAL(
            manager.model_count(near_limit, bits), SizeType(1u) << (bits - 1u));
        auto singleton = manager.constant(true);
        for (SizeType i = 0u; i != bits; ++i) {
            singleton = manager.conjunction(singleton, manager.variable(i));
        }
        ARIADNE_TESTING_EXPECT_EQUAL(manager.model_count(singleton, bits), 1u);
        // The complement has every assignment except the all-true word,
        // reaching the largest exactly representable model count.
        auto almost_all = manager.negation(singleton);
        ARIADNE_TESTING_EXPECT_EQUAL(manager.model_count(almost_all, bits), max_index);

        // Each cofactor of XOR has 2^(bits-1) satisfying assignments over
        // bits+1 variables: the individual counts fit, but their sum does not.
        auto xor_first_two = manager.exclusive_or(
            manager.variable(0u), manager.variable(1u));
        ARIADNE_TESTING_EXPECT_EQUAL(
            manager.model_count(xor_first_two, bits), SizeType(1u) << (bits - 1u));
        ARIADNE_TESTING_EXPECT_EXCEPTION(
            manager.model_count(xor_first_two, bits + 1u));

        RobddManager foreign;
        auto foreign_value = foreign.constant(true);
        ARIADNE_TESTING_EXPECT_EXCEPTION(manager.model_count(foreign_value, 0u));
        ARIADNE_TESTING_EXPECT_EXCEPTION(manager.for_each_satisfying_assignment(
            foreign_value, 0u, [](const BinaryWord&) {}));
    }

    void test_model_enumeration_truth_tables() {
        RobddManager manager;
        std::array<Robdd, 3> variables{
            manager.variable(0u), manager.variable(1u), manager.variable(2u)};
        for (unsigned mask = 0u; mask != 256u; ++mask) {
            Robdd formula = manager.constant(false);
            for (unsigned assignment = 0u; assignment != 8u; ++assignment) {
                if (((mask >> assignment) & 1u) == 0u)
                    continue;
                Robdd cube = manager.constant(true);
                for (SizeType variable = 0u; variable != 3u; ++variable) {
                    Robdd literal = ((assignment >> variable) & 1u)
                                      ? variables[variable]
                                      : manager.negation(variables[variable]);
                    cube = manager.conjunction(cube, literal);
                }
                formula = manager.disjunction(formula, cube);
            }

            std::vector<unsigned> observed;
            manager.for_each_satisfying_assignment(formula, 3u, [&](const BinaryWord& word) {
                ARIADNE_TESTING_EXPECT_EQUAL(word.size(), 3u);
                unsigned assignment = 0u;
                for (unsigned variable = 0u; variable != 3u; ++variable) {
                    if (word[variable]) {
                        assignment |= (1u << variable);
                    }
                }
                observed.push_back(assignment);
            });
            std::sort(observed.begin(), observed.end());
            std::vector<unsigned> expected;
            for (unsigned assignment = 0u; assignment != 8u; ++assignment) {
                if (((mask >> assignment) & 1u) != 0u) {
                    expected.push_back(assignment);
                }
            }
            ARIADNE_TESTING_PROVE(observed == expected);
            ARIADNE_TESTING_EXPECT_EQUAL(
                manager.model_count(formula, 3u), expected.size());

            auto negated = manager.negation(formula);
            ARIADNE_TESTING_EXPECT_EQUAL(
                manager.model_count(negated, 3u), 8u - expected.size());
            ARIADNE_TESTING_EXPECT_EQUAL(
                manager.model_count(formula, 4u), 2u * expected.size());
        }
    }

    void test_exhaustive_three_variable_truth_tables() {
        RobddManager manager;
        std::array<Robdd, 3> variables{
            manager.variable(0u), manager.variable(1u), manager.variable(2u)};
        const char* const assignments[8] = {
            "000", "100", "010", "110", "001", "101", "011", "111"};
        for (unsigned mask = 0u; mask != 256u; ++mask) {
            auto shannon = [&](auto&& self, SizeType level, unsigned prefix) -> Robdd {
                if (level == variables.size()) {
                    return manager.constant(((mask >> prefix) & 1u) != 0u);
                }
                auto low = self(self, level + 1u, prefix);
                auto high = self(self, level + 1u, prefix | (1u << level));
                return manager.ite(variables[level], high, low);
            };
            Robdd expanded = shannon(shannon, 0u, 0u);
            Robdd dnf = manager.constant(false);
            for (unsigned assignment = 0u; assignment != 8u; ++assignment) {
                if (((mask >> assignment) & 1u) == 0u)
                    continue;
                Robdd cube = manager.constant(true);
                for (SizeType variable = 0u; variable != variables.size(); ++variable) {
                    Robdd literal = ((assignment >> variable) & 1u)
                                      ? variables[variable]
                                      : manager.negation(variables[variable]);
                    cube = manager.conjunction(cube, literal);
                }
                dnf = manager.disjunction(dnf, cube);
            }
            ARIADNE_TESTING_PROVE(expanded == dnf);
            for (unsigned assignment = 0u; assignment != 8u; ++assignment) {
                ARIADNE_TESTING_EXPECT_EQUAL(
                    manager.evaluate(expanded, make_binary_word(assignments[assignment])),
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
        std::vector<Formula> formulas{
            {   manager.variable(0u), 0xAAAAu},
            {   manager.variable(1u), 0xCCCCu},
            {   manager.variable(2u), 0xF0F0u},
            {   manager.variable(3u), 0xFF00u},
            {manager.constant(false),      0u},
            { manager.constant(true), 0xFFFFu}
        };
        const char* const assignments[16] = {
            "0000", "1000", "0100", "1100", "0010", "1010", "0110", "1110",
            "0001", "1001", "0101", "1101", "0011", "1011", "0111", "1111"};
        std::uint32_t state = 0x9e3779b9u;
        auto next = [&state]() {
            state ^= state << 13u;
            state ^= state >> 17u;
            state ^= state << 5u;
            return state;
        };
        for (unsigned iteration = 0u; iteration != 160u; ++iteration) {
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
                    root = manager.conjunction(a.root, b.root);
                    expected = a.truth_table & b.truth_table;
                    break;
                case 2u:
                    root = manager.disjunction(a.root, b.root);
                    expected = a.truth_table | b.truth_table;
                    break;
                case 3u:
                    root = manager.exclusive_or(a.root, b.root);
                    expected = a.truth_table ^ b.truth_table;
                    break;
                default:
                    root = manager.ite(a.root, b.root, c.root);
                    expected = (a.truth_table & b.truth_table) | ((~a.truth_table) & c.truth_table);
                    break;
            }
            expected &= 0xFFFFu;
            for (unsigned assignment = 0u; assignment != 16u; ++assignment) {
                ARIADNE_TESTING_EXPECT_EQUAL(
                    manager.evaluate(root, make_binary_word(assignments[assignment])),
                    ((expected >> assignment) & 1u) != 0u);
            }
            formulas.push_back({root, expected});
        }
    }

    void test_evaluation() {
        RobddManager manager;
        auto x = manager.variable(0u);
        auto y = manager.variable(1u);
        auto formula = manager.exclusive_or(x, y);

        ARIADNE_TESTING_EXPECT_EQUAL(manager.evaluate(formula, make_binary_word("00")), false);
        ARIADNE_TESTING_EXPECT_EQUAL(manager.evaluate(formula, make_binary_word("01")), true);
        ARIADNE_TESTING_EXPECT_EQUAL(manager.evaluate(formula, make_binary_word("10")), true);
        ARIADNE_TESTING_EXPECT_EQUAL(manager.evaluate(formula, make_binary_word("11")), false);
        ARIADNE_TESTING_EXPECT_EXCEPTION(manager.evaluate(y, make_binary_word("0")));
    }

    void test_manager_statistics() {
        RobddManager manager;
        auto empty = manager.statistics();
        ARIADNE_TESTING_EXPECT_EQUAL(empty.node_count, 0u);
        ARIADNE_TESTING_EXPECT_EQUAL(empty.unique_table_entries, 0u);
        ARIADNE_TESTING_EXPECT_EQUAL(empty.ite_cache_entries, 0u);
        ARIADNE_TESTING_PROVE(empty.node_size_bytes != 0u);
        ARIADNE_TESTING_PROVE(empty.unique_table_entry_payload_bytes != 0u);
        ARIADNE_TESTING_PROVE(empty.ite_cache_entry_payload_bytes != 0u);

        auto x = manager.variable(0u);
        auto y = manager.variable(1u);
        auto before = manager.statistics();
        ARIADNE_TESTING_EXPECT_EQUAL(before.node_count, 2u);
        ARIADNE_TESTING_EXPECT_EQUAL(before.unique_table_entries, 2u);
        ARIADNE_TESTING_PROVE(before.node_capacity >= before.node_count);

        auto xy = manager.conjunction(x, y);
        ARIADNE_TESTING_PROVE(not xy.is_terminal());
        auto during = manager.statistics();
        ARIADNE_TESTING_PROVE(during.node_count >= before.node_count);
        ARIADNE_TESTING_EXPECT_EQUAL(
            during.unique_table_entries, during.node_count);
        ARIADNE_TESTING_PROVE(during.node_capacity >= during.node_count);
        ARIADNE_TESTING_PROVE(
            during.ite_cache_entries >= before.ite_cache_entries);
        ARIADNE_TESTING_EXPECT_EQUAL(
            manager.node_count(), during.node_count);

        // Handles do not own manager nodes; their destruction cannot
        // reclaim canonical table entries with the current manager design.
        {
            auto temporary = manager.exclusive_or(x, y);
            ARIADNE_TESTING_PROVE(not temporary.is_terminal());
        }
        auto after = manager.statistics();
        ARIADNE_TESTING_PROVE(after.node_count >= during.node_count);
        ARIADNE_TESTING_EXPECT_EQUAL(
            after.unique_table_entries, after.node_count);
        ARIADNE_TESTING_PROVE(after.node_capacity >= after.node_count);
    }

    void test_manager_separation() {
        RobddManager first;
        RobddManager second;
        auto x = first.variable(0u);
        auto y = second.variable(0u);

        ARIADNE_TESTING_PROVE(x != y);
        ARIADNE_TESTING_EXPECT_EXCEPTION(first.conjunction(x, y));
    }

    void test_parallel_ite_truth_tables_and_canonicality() {
        struct Restore {
            ThreadManager& manager;
            SizeType saved;
            ~Restore() noexcept {
                try {
                    manager.set_concurrency(saved);
                } catch (...) {
                }
            }
        };
        ThreadManager& threading = ThreadManager::instance();
        Restore restore{threading, threading.concurrency()};

        // Fresh managers for each worker setting force a genuinely cold
        // computed table. Random-looking truth tables force nontrivial ITE.
        for (SizeType workers: std::array<SizeType, 5>{0u, 1u, 2u, 4u, 8u}) {
            if (workers > threading.maximum_concurrency())
                continue;
            threading.set_concurrency(workers);
            RobddManager manager;
            constexpr unsigned bits = 7u;
            auto leaf_bit = [](SizeType assignment, SizeType salt) -> Bool {
                std::uint64_t v = static_cast<std::uint64_t>(assignment) + 0x9e3779b97f4a7c15ULL * (salt + 1u);
                v = (v ^ (v >> 30u)) * 0xbf58476d1ce4e5b9ULL;
                v = (v ^ (v >> 27u)) * 0x94d049bb133111ebULL;
                return ((v ^ (v >> 31u)) & 1u) != 0u;
            };
            auto build = [&](auto&& self, unsigned level,
                             SizeType assignment, SizeType salt) -> Robdd {
                if (level == bits)
                    return manager.constant(leaf_bit(assignment, salt));
                auto low = self(self, level + 1u, assignment, salt);
                auto high = self(self, level + 1u, assignment | (SizeType(1u) << level), salt);
                return manager.make_node(BddVariable(level), low, high);
            };
            Robdd c = build(build, 0u, 0u, 1u);
            Robdd t = build(build, 0u, 0u, 2u);
            Robdd e = build(build, 0u, 0u, 3u);
            Robdd parallel = manager.ite_parallel(c, t, e);
            Robdd serial = manager.ite(c, t, e);
            ARIADNE_TESTING_PROVE(parallel == serial);
            ARIADNE_TESTING_PROVE(manager.ite_parallel(c, t, e) == serial);
            for (SizeType assignment = 0u; assignment != (SizeType(1u) << bits); ++assignment) {
                BinaryWord word;
                for (unsigned level = 0u; level < bits; ++level)
                    word.push_back(((assignment >> level) & 1u) != 0u);
                Bool expected = manager.evaluate(c, word)
                                  ? manager.evaluate(t, word)
                                  : manager.evaluate(e, word);
                ARIADNE_TESTING_EXPECT_EQUAL(
                    manager.evaluate(parallel, word), expected);
            }
            auto not_c = manager.negation(c);
            ARIADNE_TESTING_PROVE(
                manager.ite_parallel(not_c, e, t) == serial);
            RobddManager other;
            Robdd foreign = other.constant(false);
            ARIADNE_TESTING_EXPECT_EXCEPTION(manager.ite_parallel(c, t, foreign));
        }
    }

    void test_batched_ite_cache_and_complement_semantics() {
        ThreadManager& threading = ThreadManager::instance();
        struct Restore {
            ThreadManager& manager;
            SizeType previous;
            ~Restore() noexcept {
                try {
                    manager.set_concurrency(previous);
                } catch (...) {
                }
            }
        };
        Restore restore{threading, threading.concurrency()};
        threading.set_concurrency(
            threading.maximum_concurrency() >= 2u ? 2u : 0u);
        for (unsigned first = 0u; first < 2u; ++first) {
            RobddManager manager;
            auto f = manager.constant(false);
            auto t = manager.constant(true);
            auto x = manager.variable(0u);
            auto y = manager.variable(2u);
            auto z = manager.variable(4u);
            auto xy = manager.exclusive_or(x, y);
            std::vector<Robdd> choices{f, t, x, y, z, manager.negation(x),
                                       manager.negation(y), manager.negation(z), xy,
                                       manager.negation(xy)};
            for (const auto& c: choices)
                for (const auto& a: choices)
                    for (const auto& b: choices) {
                        Robdd lhs = first == 0u
                                      ? manager.ite(c, a, b)
                                      : manager.ite_parallel(c, a, b);
                        Robdd rhs = first == 0u
                                      ? manager.ite_parallel(c, a, b)
                                      : manager.ite(c, a, b);
                        ARIADNE_TESTING_PROVE(lhs == rhs);
                        for (SizeType mask = 0u; mask < 32u; ++mask) {
                            BinaryWord word;
                            for (unsigned bit = 0u; bit < 5u; ++bit)
                                word.push_back(((mask >> bit) & 1u) != 0u);
                            Bool expected = manager.evaluate(c, word)
                                              ? manager.evaluate(a, word)
                                              : manager.evaluate(b, word);
                            ARIADNE_TESTING_EXPECT_EQUAL(
                                manager.evaluate(lhs, word), expected);
                        }
                    }
        }
    }

    void test_model_count_skipped_level_overflow() {
        RobddManager manager;
        constexpr SizeType bits = std::numeric_limits<SizeType>::digits;
        // Both terminal cofactors fit individually. The skipped variables
        // make the final count overflow, either by the shift width itself
        // or by multiplying a representable subtotal by two.
        auto distant = manager.variable(bits);
        ARIADNE_TESTING_EXPECT_EXCEPTION(manager.model_count(distant, bits + 1u));
        auto second = manager.variable(1u);
        ARIADNE_TESTING_EXPECT_EXCEPTION(manager.model_count(second, bits + 1u));
        ARIADNE_TESTING_EXPECT_EQUAL(manager.model_count(second, 3u), 4u);
    }

    void test_ite_profiled_reductions() {
        RobddManager manager;
        auto f = manager.constant(false);
        auto t = manager.constant(true);
        auto x = manager.variable(0u);
        auto y = manager.variable(1u);
        auto not_x = manager.negation(x);
        RobddIteProfile counters;

        ARIADNE_TESTING_EXPECT_EQUAL(manager.ite_profiled(t, x, y, false, counters), x);
        ARIADNE_TESTING_EXPECT_EQUAL(manager.ite_profiled(f, x, y, false, counters), y);
        ARIADNE_TESTING_EXPECT_EQUAL(manager.ite_profiled(x, y, y, false, counters), y);
        ARIADNE_TESTING_EXPECT_EQUAL(manager.ite_profiled(x, t, f, false, counters), x);
        ARIADNE_TESTING_EXPECT_EQUAL(manager.ite_profiled(x, f, t, false, counters), not_x);
        ARIADNE_TESTING_EXPECT_EQUAL(manager.ite_profiled(x, t, y, false, counters),
                                     manager.disjunction(x, y));
        ARIADNE_TESTING_EXPECT_EQUAL(manager.ite_profiled(x, f, y, false, counters),
                                     manager.conjunction(not_x, y));

        // ITE(x XOR y, y, NOT x) = NOT x. Its y cofactors reduce to
        // equal children; the surviving x node has a complemented low edge.
        auto condition = manager.exclusive_or(x, y);
        ARIADNE_TESTING_EXPECT_EQUAL(
            manager.ite_profiled(condition, y, not_x, false, counters), not_x);
        ARIADNE_TESTING_PROVE(counters.calls > 1u);
    }

    void test_ite_profiled_concurrent_cache_publication() {
        RobddManager manager;
        // Build the operands through canonical nodes, without populating
        // the ITE cache that the concurrent calls are about to share.
        std::uint32_t state = 0x9e3779b9u;
        std::function<Robdd(SizeType)> operand = [&](SizeType index) {
            if (index == 10u) {
                state ^= state << 13u;
                state ^= state >> 17u;
                state ^= state << 5u;
                return manager.constant((state & 1u) != 0u);
            }
            auto low = operand(index + 1u);
            auto high = operand(index + 1u);
            return manager.make_node(BddVariable(index), low, high);
        };
        auto condition = operand(0u);
        auto then_branch = operand(0u);
        auto else_branch = operand(0u);
        constexpr SizeType workers = 8u;
        std::barrier start(static_cast<std::ptrdiff_t>(workers));
        std::vector<std::future<std::pair<Robdd, RobddIteProfile>>> results;
        for (SizeType worker = 0u; worker < workers; ++worker) {
            results.push_back(std::async(std::launch::async, [&] {
                RobddIteProfile profile;
                start.arrive_and_wait();
                auto result = manager.ite_profiled(
                    condition, then_branch, else_branch, false, profile);
                return std::make_pair(result, profile);
            }));
        }
        auto expected = results.front().get().first;
        for (SizeType worker = 1u; worker < workers; ++worker) {
            auto result = results[worker].get();
            ARIADNE_TESTING_EXPECT_EQUAL(result.first, expected);
            ARIADNE_TESTING_PROVE(
                result.second.cache_raced_inserts <= result.second.cache_lookups);
            ARIADNE_TESTING_PROVE(
                result.second.cache_lock_contended <= result.second.cache_lock_calls);
        }
        ARIADNE_TESTING_EXPECT_EQUAL(
            manager.ite(condition, then_branch, else_branch), expected);
        for (SizeType assignment = 0u; assignment < 1024u; ++assignment) {
            BinaryWord word;
            for (SizeType variable = 0u; variable < 10u; ++variable)
                word.append(((assignment >> variable) & 1u) != 0u);
            const Bool truth = manager.evaluate(condition, word)
                                 ? manager.evaluate(then_branch, word)
                                 : manager.evaluate(else_branch, word);
            ARIADNE_TESTING_EXPECT_EQUAL(manager.evaluate(expected, word), truth);
        }
    }

    void test_ite_profiled_diagnostics() {
        struct Restore {
            ThreadManager& manager;
            SizeType value;
            ~Restore() noexcept {
                try {
                    manager.set_concurrency(value);
                } catch (...) {
                }
            }
        };
        ThreadManager& threading = ThreadManager::instance();
        Restore restore{threading, threading.concurrency()};

        for (SizeType workers: std::array<SizeType, 4>{0u, 1u, 2u, 4u}) {
            if (workers > threading.maximum_concurrency())
                continue;
            threading.set_concurrency(workers);
            RobddManager manager;
            auto x = manager.variable(0u);
            auto y = manager.variable(1u);
            auto z = manager.variable(2u);
            auto condition = manager.exclusive_or(x, y);
            auto then_branch = manager.exclusive_or(y, z);
            auto else_branch = manager.disjunction(x, z);

            RobddIteProfile counters;
            Robdd actual = manager.ite_profiled(
                condition, then_branch, else_branch, true, counters);
            Robdd expected = manager.ite(condition, then_branch, else_branch);
            ARIADNE_TESTING_PROVE(actual == expected);
            ARIADNE_TESTING_PROVE(counters.calls > 0u);
            ARIADNE_TESTING_PROVE(
                counters.cache_lock_calls >= counters.cache_lookups);
            ARIADNE_TESTING_EXPECT_EQUAL(counters.node_lock_calls, 0u);
            ARIADNE_TESTING_PROVE(
                counters.node_snapshot_reads <= 3u * counters.cache_lookups);
            ARIADNE_TESTING_PROVE(
                counters.cache_lock_contended <= counters.cache_lock_calls);
            ARIADNE_TESTING_PROVE(
                counters.node_lock_contended <= counters.node_lock_calls);
            ARIADNE_TESTING_PROVE(
                counters.unique_lock_contended <= counters.unique_lock_calls);
            ARIADNE_TESTING_PROVE(
                counters.cache_hits <= counters.cache_lookups);
            if (workers < 2u) {
                ARIADNE_TESTING_EXPECT_EQUAL(counters.tasks_forked, 0u);
                ARIADNE_TESTING_EXPECT_EQUAL(counters.cache_raced_inserts, 0u);
            }
            RobddIteProfile warm;
            ARIADNE_TESTING_PROVE(
                manager.ite_profiled(
                    condition, then_branch, else_branch, false, warm) == expected);
            ARIADNE_TESTING_PROVE(warm.cache_hits > 0u);
            ARIADNE_TESTING_EXPECT_EQUAL(warm.tasks_forked, 0u);

            RobddManager other;
            ARIADNE_TESTING_EXPECT_EXCEPTION(manager.ite_profiled(
                condition, then_branch, other.constant(false),
                true, counters));
        }
    }

    void test_node_readers_during_concurrent_growth() {
        // Repeated reallocations of the physical node vector must not race
        // with reads of already published canonical node references.
        auto& threading = ThreadManager::instance();
        const SizeType maximum = threading.maximum_concurrency();
        if (maximum < 2u)
            return;

        struct Restore {
            ThreadManager& manager;
            SizeType saved;
            ~Restore() noexcept {
                try {
                    manager.set_concurrency(saved);
                } catch (...) {
                }
            }
        };
        Restore restore{threading, threading.concurrency()};
        threading.set_concurrency(std::min<SizeType>(maximum, 4u));

        RobddManager manager;
        const Robdd x = manager.variable(0u);
        const Robdd y = manager.variable(1u);
        const Robdd xy = manager.conjunction(x, y);
        const BinaryWord assignment = make_binary_word("11");

        std::vector<std::future<void>> futures;
        for (SizeType writer = 0u; writer < 4u; ++writer) {
            futures.push_back(threading.enqueue([&, writer] {
                for (SizeType i = 0u; i < 256u; ++i) {
                    Robdd v = manager.variable(2u + writer * 256u + i);
                    Robdd combined = manager.conjunction(x, v);
                    if (not manager.top_variable(combined).has_value())
                        throw std::runtime_error("Lost canonical root");
                }
            }));
        }
        for (SizeType reader = 0u; reader < 4u; ++reader) {
            futures.push_back(threading.enqueue([&] {
                for (SizeType i = 0u; i < 256u; ++i) {
                    if (not manager.evaluate(xy, assignment))
                        throw std::runtime_error("Concurrent node read changed truth");
                    if (not manager.low(xy).is_false())
                        throw std::runtime_error("Concurrent low cofactor changed");
                    if (manager.high(xy) != y)
                        throw std::runtime_error("Concurrent high cofactor changed");
                    const auto snapshot = manager.statistics();
                    if (snapshot.unique_table_entries != snapshot.node_count)
                        throw std::runtime_error(
                            "Manager snapshot observed partial canonical insertion");
                    if (manager.top_variable(xy)->index() != 0u)
                        throw std::runtime_error("Concurrent root variable changed");
                }
            }));
        }
        for (auto& future: futures)
            future.get();

        const auto final_stats = manager.statistics();
        ARIADNE_TESTING_EXPECT_EQUAL(
            final_stats.unique_table_entries, final_stats.node_count);
        ARIADNE_TESTING_PROVE(manager.evaluate(xy, assignment));
        for (SizeType writer = 0u; writer < 4u; ++writer)
            for (SizeType i = 0u; i < 256u; ++i)
                ARIADNE_TESTING_PROVE(
                    manager.top_variable(manager.variable(2u + writer * 256u + i))
                        ->index() == 2u + writer * 256u + i);
    }

    void test_hashed_unique_table_concurrent_canonicality() {
        auto& threading = ThreadManager::instance();
        const SizeType max_workers = threading.maximum_concurrency();
        // The same canonical keys are submitted simultaneously from
        // independent workers; no duplicate nodes may be allocated.
        struct RestoreConcurrency {
            ThreadManager& manager;
            SizeType previous;
            ~RestoreConcurrency() noexcept {
                try {
                    manager.set_concurrency(previous);
                } catch (...) {
                }
            }
        };
        RestoreConcurrency restore{threading, threading.concurrency()};
        if (max_workers >= 2u)
            threading.set_concurrency(std::min<SizeType>(4u, max_workers));
        else
            threading.set_concurrency(0u);

        RobddManager manager;
        const Robdd low = manager.variable(512u);
        const Robdd high = manager.variable(513u);
        const Robdd complement_low = manager.negation(low);
        const Robdd complement_high = manager.negation(high);

        // Collision safety and canonicality depend on the full tuple
        // (variable index, low ref, high ref), including complement bits.
        std::vector<std::future<void>> futures;
        for (unsigned worker = 0u; worker < 8u; ++worker) {
            futures.push_back(threading.enqueue([&manager, low, high,
                                                 complement_low, complement_high] {
                for (SizeType i = 0u; i < 128u; ++i) {
                    auto a = manager.make_node(BddVariable(i), low, high);
                    auto b = manager.make_node(BddVariable(i), high, low);
                    auto c = manager.make_node(
                        BddVariable(i), complement_low, complement_high);
                    if (c != manager.negation(a) or a == b)
                        throw std::runtime_error("Hashed unique table lost canonicality");
                }
            }));
        }
        for (auto& f: futures)
            f.get();

        const auto stats = manager.statistics();
        ARIADNE_TESTING_EXPECT_EQUAL(stats.node_count, 258u);
        ARIADNE_TESTING_EXPECT_EQUAL(stats.unique_table_entries, 258u);
        // Reconstruct every key again: repeated insertion must not
        // allocate anything and returned handles must stay canonical.
        for (SizeType i = 0u; i < 128u; ++i) {
            auto a = manager.make_node(BddVariable(i), low, high);
            auto b = manager.make_node(BddVariable(i), high, low);
            ARIADNE_TESTING_PROVE(a != b);
            ARIADNE_TESTING_PROVE(manager.negation(a) == manager.make_node(
                                                             BddVariable(i), complement_low, complement_high));
        }
        ARIADNE_TESTING_EXPECT_EQUAL(manager.node_count(), 258u);
    }

    void test_concurrent_canonical_construction() {
        auto& thread_manager = ThreadManager::instance();
        SizeType maximum = thread_manager.maximum_concurrency();
        if (maximum == 0u)
            return;

        SizeType previous = thread_manager.concurrency();
        thread_manager.set_concurrency(std::min<SizeType>(maximum, 4u));

        RobddManager manager;
        auto x = manager.variable(0u);
        auto y = manager.variable(1u);
        auto not_y = manager.negation(y);

        std::vector<std::future<Robdd>> futures;
        for (SizeType i = 0u; i != 32u; ++i) {
            futures.push_back(thread_manager.enqueue([&manager, x, y, not_y] {
                auto lhs = manager.conjunction(x, y);
                auto rhs = manager.conjunction(x, not_y);
                return manager.disjunction(lhs, rhs);
            }));
        }

        for (auto& future: futures) {
            ARIADNE_TESTING_PROVE(future.get() == x);
        }

        thread_manager.set_concurrency(previous);
    }

    void test_threading_command_line() {
        ThreadingCommandLineInterface cli;
        ARIADNE_TESTING_PROVE(cli.acquire({""}));
        ARIADNE_TESTING_PROVE(cli.acquire({"", "--verbosity", "2", "--theme", "none", "--scheduler", "immediate", "--concurrency", "0"}));
        ARIADNE_TESTING_EXPECT_EQUAL(Logger::instance().configuration().verbosity(), 2u);
        ARIADNE_TESTING_EXPECT_EQUAL(ThreadManager::instance().concurrency(), 0u);
        ARIADNE_TESTING_PROVE(not cli.acquire({"", "--concurrency", "invalid"}));
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
    ARIADNE_TESTING_ADD_TEST(TestRobdd, test_manager_statistics);
    ARIADNE_TESTING_ADD_TEST(TestRobdd, test_manager_separation);
    ARIADNE_TESTING_ADD_TEST(TestRobdd, test_concurrent_canonical_construction);
    ARIADNE_TESTING_ADD_TEST(TestRobdd, test_hashed_unique_table_concurrent_canonicality);
    ARIADNE_TESTING_ADD_TEST(TestRobdd, test_node_readers_during_concurrent_growth);
    ARIADNE_TESTING_ADD_TEST(TestRobdd, test_parallel_ite_truth_tables_and_canonicality);
    ARIADNE_TESTING_ADD_TEST(TestRobdd, test_ite_profiled_diagnostics);
    ARIADNE_TESTING_ADD_TEST(TestRobdd, test_ite_profiled_reductions);
    ARIADNE_TESTING_ADD_TEST(TestRobdd, test_ite_profiled_concurrent_cache_publication);
    ARIADNE_TESTING_ADD_TEST(TestRobdd, test_model_count_skipped_level_overflow);
    ARIADNE_TESTING_ADD_TEST(TestRobdd, test_batched_ite_cache_and_complement_semantics);
    return ARIADNE_TESTING_RUN_TESTS_WITH_ARGS(argc, argv);
}
