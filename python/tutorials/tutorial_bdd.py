#!/usr/bin/python3

##############################################################################
#            tutorial_bdd.py
#
#  Copyright  2026  Luca Geretti
##############################################################################

# This file is part of Ariadne BDD.
#
# Ariadne BDD is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

from pyariadne import BddManager, BinaryWord


def section(title):
    print(f"\n=== {title} ===")


def tutorial_bdd():
    section("Manager, constants and variables")

    manager = BddManager()

    zero = manager.constant(False)
    one = manager.constant(True)
    x = manager.variable(0)
    y = manager.variable(1)
    z = manager.variable(2)

    print(f"false is terminal: {zero.is_terminal()}")
    print(f"true is terminal:  {one.is_terminal()}")
    print(f"initial non-terminal nodes: {manager.node_count()}")

    x_again = manager.variable(0)
    print(f"x is canonical: {x == x_again}")

    if x != x_again:
        raise RuntimeError("Canonical variable construction failed.")

    section("Boolean operations and canonicity")

    xy = manager.conjunction(x, y)
    yx = manager.conjunction(y, x)
    x_or_not_x = manager.disjunction(x, manager.negation(x))

    print(f"x AND y == y AND x: {xy == yx}")
    print(f"x OR NOT x is true: {x_or_not_x.is_true()}")

    if xy != yx or not x_or_not_x.is_true():
        raise RuntimeError("A Boolean identity failed.")

    section("Multiplexer")

    formula = manager.disjunction(
        manager.conjunction(x, y),
        manager.conjunction(manager.negation(x), z),
    )

    mux = manager.ite(x, y, z)

    print(f"expanded formula == ITE(x,y,z): {formula == mux}")
    print(f"non-terminal nodes after construction: {manager.node_count()}")

    if formula != mux:
        raise RuntimeError("Canonical equivalence with ITE failed.")

    section("Truth table")

    assignments = (
        "000", "001", "010", "011",
        "100", "101", "110", "111",
    )

    for bits in assignments:
        assignment = BinaryWord(bits)
        actual = manager.evaluate(formula, assignment)
        expected = bits[1] == "1" if bits[0] == "1" else bits[2] == "1"

        print(f"x,y,z={bits} -> {actual}")

        if actual != expected:
            raise RuntimeError(f"Truth-table mismatch for assignment {bits}.")

    section("Reduction")

    reduced = manager.ite(x, y, y)
    print(f"ITE(x,y,y) == y: {reduced == y}")

    if reduced != y:
        raise RuntimeError("ROBDD reduction failed.")

    print("\nAriadne BDD tutorial completed successfully.")


if __name__ == "__main__":
    tutorial_bdd()
