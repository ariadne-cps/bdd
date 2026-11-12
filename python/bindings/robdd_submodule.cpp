/***************************************************************************
 *            python/bindings/robdd_submodule.cpp
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

/** @file python/bindings/robdd_submodule.cpp
 *  @brief Expose ROBDD handles, managers and operations to Python.
 */

#include <memory>
#include <stdexcept>

#include "bdd/robdd.hpp"
#include "pybind11.hpp"

using namespace Ariadne;

void robdd_submodule(pybind11::module& module) {
    pybind11::class_<Robdd>(module, "Robdd")
        .def("is_false", &Robdd::is_false)
        .def("is_true", &Robdd::is_true)
        .def("is_terminal", &Robdd::is_terminal)
        .def("__eq__", [](const Robdd& lhs, const Robdd& rhs) {
            return lhs == rhs;
        })
        .def("__ne__", [](const Robdd& lhs, const Robdd& rhs) {
            return lhs != rhs;
        });

    pybind11::class_<RobddManager, std::shared_ptr<RobddManager>>(module, "RobddManager")
        .def(pybind11::init<>())
        .def("constant", [](RobddManager& manager, Bool value) { return manager.constant(value); }, pybind11::keep_alive<0, 1>())
        .def("variable", [](RobddManager& manager, SizeType index) { return manager.variable(index); }, pybind11::keep_alive<0, 1>())
        .def("variable", [](RobddManager& manager, BddVariable variable) { return manager.variable(variable); }, pybind11::keep_alive<0, 1>())
        .def("top_variable", &RobddManager::top_variable)
        .def("low", [](const RobddManager& manager, const Robdd& value) { return manager.low(value); }, pybind11::keep_alive<0, 1>())
        .def("high", [](const RobddManager& manager, const Robdd& value) { return manager.high(value); }, pybind11::keep_alive<0, 1>())
        .def("make_node", [](RobddManager& manager, BddVariable variable, const Robdd& low, const Robdd& high) { return manager.make_node(variable, low, high); }, pybind11::keep_alive<0, 1>())
        .def("support", &RobddManager::support)
        .def("shift", [](RobddManager& manager, const Robdd& value, std::ptrdiff_t offset) { return manager.shift(value, offset); }, pybind11::keep_alive<0, 1>())
        .def("rename", [](RobddManager& manager, const Robdd& value, const std::map<SizeType, SizeType>& indices) { return manager.rename(value, indices); }, pybind11::keep_alive<0, 1>())
        .def("restrict", [](RobddManager& manager, const Robdd& value, BddVariable variable, Bool assignment) { return manager.restrict(value, variable, assignment); }, pybind11::keep_alive<0, 1>())
        .def("exists", [](RobddManager& manager, const Robdd& value, const std::vector<BddVariable>& variables) { return manager.exists(value, variables); }, pybind11::keep_alive<0, 1>())
        .def("and_exists", [](RobddManager& manager, const Robdd& lhs, const Robdd& rhs, const std::vector<BddVariable>& variables) { return manager.and_exists(lhs, rhs, variables); }, pybind11::keep_alive<0, 1>())
        .def("negation", [](RobddManager& manager, const Robdd& value) { return manager.negation(value); }, pybind11::keep_alive<0, 1>())
        .def("conjunction", [](RobddManager& manager, const Robdd& lhs, const Robdd& rhs) { return manager.conjunction(lhs, rhs); }, pybind11::keep_alive<0, 1>())
        .def("disjunction", [](RobddManager& manager, const Robdd& lhs, const Robdd& rhs) { return manager.disjunction(lhs, rhs); }, pybind11::keep_alive<0, 1>())
        .def("exclusive_or", [](RobddManager& manager, const Robdd& lhs, const Robdd& rhs) { return manager.exclusive_or(lhs, rhs); }, pybind11::keep_alive<0, 1>())
        .def("ite", [](RobddManager& manager, const Robdd& condition, const Robdd& then_branch, const Robdd& else_branch) { return manager.ite(condition, then_branch, else_branch); }, pybind11::keep_alive<0, 1>())
        .def("evaluate", &RobddManager::evaluate)
        .def("model_count", &RobddManager::model_count, pybind11::arg("value"), pybind11::arg("variable_count"), pybind11::arg("first_variable_index") = 0u)
        .def("satisfying_assignments", [](const RobddManager& manager, const Robdd& value, SizeType variable_count, SizeType max_assignments, SizeType first_variable_index) {
                SizeType count = manager.model_count(value,variable_count,first_variable_index);
                if (count > max_assignments) {
                    throw std::length_error("Satisfying-assignment enumeration exceeds max_assignments");
                }
                std::vector<BinaryWord> assignments;
                assignments.reserve(count);
                manager.for_each_satisfying_assignment(value,variable_count,[&]( const BinaryWord & assignment) {
                    assignments.push_back(assignment);
                },first_variable_index);
                return assignments; }, pybind11::arg("value"), pybind11::arg("variable_count"), pybind11::arg("max_assignments"), pybind11::arg("first_variable_index") = 0u)
        .def("node_count", &RobddManager::node_count);
}
