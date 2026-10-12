/***************************************************************************
 *            robdd_submodule.cpp
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

#include "pybind11.hpp"
#include "bdd/robdd.hpp"

using namespace Ariadne;

void robdd_submodule(pybind11::module& module) {
    pybind11::class_<Robdd>(module,"Robdd")
        .def("is_false",&Robdd::is_false)
        .def("is_true",&Robdd::is_true)
        .def("is_terminal",&Robdd::is_terminal)
        .def("__eq__",[](Robdd const& lhs, Robdd const& rhs) {
            return lhs==rhs;
        })
        .def("__ne__",[](Robdd const& lhs, Robdd const& rhs) {
            return lhs!=rhs;
        });

    pybind11::class_<RobddManager>(module,"RobddManager")
        .def(pybind11::init<>())
        .def("constant",
            [](RobddManager& manager, Bool value) {
                return manager.constant(value);
            },
            pybind11::keep_alive<0,1>())
        .def("variable",
            [](RobddManager& manager, SizeType index) {
                return manager.variable(index);
            },
            pybind11::keep_alive<0,1>())
        .def("variable",
            [](RobddManager& manager, BddVariable variable) {
                return manager.variable(variable);
            },
            pybind11::keep_alive<0,1>())
        .def("negation",
            [](RobddManager& manager, Robdd const& value) {
                return manager.negation(value);
            },
            pybind11::keep_alive<0,1>())
        .def("conjunction",
            [](RobddManager& manager, Robdd const& lhs, Robdd const& rhs) {
                return manager.conjunction(lhs,rhs);
            },
            pybind11::keep_alive<0,1>())
        .def("disjunction",
            [](RobddManager& manager, Robdd const& lhs, Robdd const& rhs) {
                return manager.disjunction(lhs,rhs);
            },
            pybind11::keep_alive<0,1>())
        .def("exclusive_or",
            [](RobddManager& manager, Robdd const& lhs, Robdd const& rhs) {
                return manager.exclusive_or(lhs,rhs);
            },
            pybind11::keep_alive<0,1>())
        .def("ite",
            [](RobddManager& manager, Robdd const& condition,
               Robdd const& then_branch, Robdd const& else_branch) {
                return manager.ite(condition,then_branch,else_branch);
            },
            pybind11::keep_alive<0,1>())
        .def("evaluate",&RobddManager::evaluate)
        .def("node_count",&RobddManager::node_count);
}
