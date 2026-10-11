/***************************************************************************
 *            bdd_module.cpp
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

void bdd_submodule(pybind11::module& module);

PYBIND11_MODULE(pyariadne, module) {
    bdd_submodule(module);
}
