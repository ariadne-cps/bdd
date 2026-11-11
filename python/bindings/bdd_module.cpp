/***************************************************************************
 *            python/bindings/bdd_module.cpp
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

/** @file python/bindings/bdd_module.cpp
 *  @brief Register the BDD extension and its Python submodules.
 */

#include "pybind11.hpp"

void bdd_variable_submodule(pybind11::module& module);
void robdd_submodule(pybind11::module& module);

void bdd_module(pybind11::module& module) {
    bdd_variable_submodule(module);
    robdd_submodule(module);
}
