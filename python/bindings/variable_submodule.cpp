/***************************************************************************
 *            python/bindings/variable_submodule.cpp
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

/** @file python/bindings/variable_submodule.cpp
 *  @brief Expose Boolean variable identifiers and binary words to Python.
 */

#include <sstream>

#include "bdd/variable.hpp"
#include "pybind11.hpp"
#include "utility/binary_word.hpp"

using namespace Ariadne;

void bdd_variable_submodule(pybind11::module& module) {
    pybind11::class_<BinaryWord>(module, "BinaryWord")
        .def(pybind11::init<StringType>())
        .def("__len__", [](const BinaryWord& word) { return word.size(); })
        .def("__str__", [](const BinaryWord& word) {
            std::ostringstream stream;
            stream << word;
            return stream.str();
        });

    pybind11::class_<BddVariable>(module, "BddVariable")
        .def(pybind11::init<SizeType>())
        .def("index", &BddVariable::index)
        .def("__eq__", [](const BddVariable& lhs, const BddVariable& rhs) {
            return lhs == rhs;
        })
        .def("__ne__", [](const BddVariable& lhs, const BddVariable& rhs) {
            return lhs != rhs;
        });
}
