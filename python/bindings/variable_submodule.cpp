/***************************************************************************
 *            variable_submodule.cpp
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
#include "utility/binary_word.hpp"
#include "bdd/variable.hpp"

using namespace Ariadne;

void bdd_variable_submodule(pybind11::module& module) {
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
}
