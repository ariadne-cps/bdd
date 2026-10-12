/***************************************************************************
 *            variable.hpp
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

#ifndef ARIADNE_BDD_VARIABLE_HPP
#define ARIADNE_BDD_VARIABLE_HPP

#include "utility/typedefs.hpp"

namespace Ariadne {

//! \brief Identifier of a Boolean variable used by decision diagrams.
//! \details Variable identity is representation-independent. A particular
//! decision-diagram implementation may map it to a distinct ordering level.
class BddVariable {
  public:
    explicit BddVariable(SizeType index) : _index(index) { }

    SizeType index() const { return _index; }

    friend Bool operator==(BddVariable lhs, BddVariable rhs) { return lhs._index == rhs._index; }
    friend Bool operator!=(BddVariable lhs, BddVariable rhs) { return not (lhs == rhs); }
    friend Bool operator<(BddVariable lhs, BddVariable rhs) { return lhs._index < rhs._index; }

  private:
    SizeType _index;
};

} // namespace Ariadne

#endif // ARIADNE_BDD_VARIABLE_HPP
