/***************************************************************************
 *            tutorial_bdd.cpp
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

#include <array>
#include <iostream>

#include "ariadne-bdd.hpp"

using namespace Ariadne;

namespace {

void section(const char* title) {
    std::cout << "\n=== " << title << " ===\n";
}

} // namespace

int main() {
    std::cout << std::boolalpha;

    section("Manager, constants and variables");

    BddManager manager;

    Bdd zero=manager.constant(false);
    Bdd one=manager.constant(true);
    Bdd x=manager.variable(0u);
    Bdd y=manager.variable(1u);
    Bdd z=manager.variable(2u);

    std::cout << "false is terminal: " << zero.is_terminal() << "\n";
    std::cout << "true is terminal:  " << one.is_terminal() << "\n";
    std::cout << "initial non-terminal nodes: " << manager.node_count() << "\n";

    Bdd x_again=manager.variable(0u);
    std::cout << "x is canonical: " << (x==x_again) << "\n";

    if(x!=x_again) {
        std::cerr << "Canonical variable construction failed.\n";
        return 1;
    }

    section("Boolean operations and canonicity");

    Bdd xy=manager.conjunction(x,y);
    Bdd yx=manager.conjunction(y,x);
    Bdd x_or_not_x=manager.disjunction(x,manager.negation(x));

    std::cout << "x AND y == y AND x: " << (xy==yx) << "\n";
    std::cout << "x OR NOT x is true: " << x_or_not_x.is_true() << "\n";

    if(xy!=yx || !x_or_not_x.is_true()) {
        std::cerr << "A Boolean identity failed.\n";
        return 1;
    }

    section("Multiplexer");

    Bdd formula=manager.disjunction(
        manager.conjunction(x,y),
        manager.conjunction(manager.negation(x),z));

    Bdd mux=manager.ite(x,y,z);

    std::cout << "expanded formula == ITE(x,y,z): " << (formula==mux) << "\n";
    std::cout << "non-terminal nodes after construction: "
              << manager.node_count() << "\n";

    if(formula!=mux) {
        std::cerr << "Canonical equivalence with ITE failed.\n";
        return 1;
    }

    section("Truth table");

    std::array<const char*,8> assignments={
        "000","001","010","011",
        "100","101","110","111"
    };

    for(const char* bits : assignments) {
        BinaryWord assignment(bits);
        Bool actual=manager.evaluate(formula,assignment);
        Bool expected=(bits[0]=='1') ? (bits[1]=='1') : (bits[2]=='1');

        std::cout << "x,y,z=" << bits << " -> " << actual << "\n";

        if(actual!=expected) {
            std::cerr << "Truth-table mismatch for assignment " << bits << ".\n";
            return 1;
        }
    }

    section("Reduction");

    Bdd reduced=manager.ite(x,y,y);
    std::cout << "ITE(x,y,y) == y: " << (reduced==y) << "\n";

    if(reduced!=y) {
        std::cerr << "ROBDD reduction failed.\n";
        return 1;
    }

    std::cout << "\nAriadne BDD tutorial completed successfully.\n";
    return 0;
}
