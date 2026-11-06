/***************************************************************************
 *            robdd.hpp
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

#ifndef ARIADNE_BDD_ROBDD_HPP
#define ARIADNE_BDD_ROBDD_HPP

#include "bdd/detail/robdd_impl.hpp"

namespace Ariadne {

//! \brief Canonical ROBDD handle using complemented edges internally.
using Robdd = Detail::ComplementedRobdd;

//! \brief Canonical ROBDD manager using complemented edges internally.
//! \details The edge policy is fixed at compile time; there is no runtime
//! complemented-edge feature branch in the public manager hot path.
using RobddManager = Detail::ComplementedRobddManager;

} // namespace Ariadne

#endif // ARIADNE_BDD_ROBDD_HPP
