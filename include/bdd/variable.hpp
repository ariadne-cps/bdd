/***************************************************************************
 *            bdd/variable.hpp
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

/** @file bdd/variable.hpp
 *  @brief Representation-independent identifiers of Boolean decision variables.
 */

#ifndef ARIADNE_BDD_VARIABLE_HPP
#define ARIADNE_BDD_VARIABLE_HPP

#include "utility/typedefs.hpp"

namespace Ariadne {

//! \brief Identifier of a Boolean variable used by decision diagrams.
//! \details Variable identity is representation-independent. A particular
//! decision-diagram implementation may map it to a distinct ordering level.
class BddVariable {
  public:
    /** @brief Construct a Boolean variable with the given stable index. */
    explicit BddVariable(SizeType index) : _index(index) {}

    /** @brief Return the Boolean variable index. */
    SizeType index() const { return _index; }

    /** @brief Compare Boolean variables for identity. */
    friend Bool operator==(BddVariable lhs, BddVariable rhs) { return lhs._index == rhs._index; }
    /** @brief Check whether two variables have distinct indices. */
    friend Bool operator!=(BddVariable lhs, BddVariable rhs) { return not(lhs == rhs); }
    /** @brief Compare variable indices in increasing order. */
    friend Bool operator<(BddVariable lhs, BddVariable rhs) { return lhs._index < rhs._index; }

  private:
    SizeType _index;
};

} // namespace Ariadne

#endif // ARIADNE_BDD_VARIABLE_HPP
