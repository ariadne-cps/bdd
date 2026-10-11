/***************************************************************************
 *            bdd_submodule.cpp
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

#include <sstream>

#include "pybind11.hpp"
#include "ariadne-bdd.hpp"

using namespace Ariadne;

void bdd_submodule(pybind11::module& module) {
    pybind11::class_<BinaryWord>(module,"BinaryWord")
        .def(pybind11::init<StringType>())
        .def("__len__",[](BinaryWord const& word) { return word.size(); })
        .def("__str__",[](BinaryWord const& word) {
            std::ostringstream stream;
            stream << word;
            return stream.str();
        });

    pybind11::class_<BddVariable>(module,"BddVariable")
        .def(pybind11::init<SizeType>())
        .def("index",&BddVariable::index)
        .def("__eq__",[](BddVariable const& lhs, BddVariable const& rhs) {
            return lhs==rhs;
        })
        .def("__ne__",[](BddVariable const& lhs, BddVariable const& rhs) {
            return lhs!=rhs;
        });

    pybind11::class_<Bdd>(module,"Bdd")
        .def("is_false",&Bdd::is_false)
        .def("is_true",&Bdd::is_true)
        .def("is_terminal",&Bdd::is_terminal)
        .def("__eq__",[](Bdd const& lhs, Bdd const& rhs) {
            return lhs==rhs;
        })
        .def("__ne__",[](Bdd const& lhs, Bdd const& rhs) {
            return lhs!=rhs;
        });

    pybind11::class_<BddManager>(module,"BddManager")
        .def(pybind11::init<>())
        .def("constant",
            [](BddManager& manager, Bool value) {
                return manager.constant(value);
            },
            pybind11::keep_alive<0,1>())
        .def("variable",
            [](BddManager& manager, SizeType index) {
                return manager.variable(index);
            },
            pybind11::keep_alive<0,1>())
        .def("variable",
            [](BddManager& manager, BddVariable variable) {
                return manager.variable(variable);
            },
            pybind11::keep_alive<0,1>())
        .def("negation",
            [](BddManager& manager, Bdd const& value) {
                return manager.negation(value);
            },
            pybind11::keep_alive<0,1>())
        .def("conjunction",
            [](BddManager& manager, Bdd const& lhs, Bdd const& rhs) {
                return manager.conjunction(lhs,rhs);
            },
            pybind11::keep_alive<0,1>())
        .def("disjunction",
            [](BddManager& manager, Bdd const& lhs, Bdd const& rhs) {
                return manager.disjunction(lhs,rhs);
            },
            pybind11::keep_alive<0,1>())
        .def("exclusive_or",
            [](BddManager& manager, Bdd const& lhs, Bdd const& rhs) {
                return manager.exclusive_or(lhs,rhs);
            },
            pybind11::keep_alive<0,1>())
        .def("ite",
            [](BddManager& manager, Bdd const& condition,
               Bdd const& then_branch, Bdd const& else_branch) {
                return manager.ite(condition,then_branch,else_branch);
            },
            pybind11::keep_alive<0,1>())
        .def("evaluate",&BddManager::evaluate)
        .def("node_count",&BddManager::node_count);
}
