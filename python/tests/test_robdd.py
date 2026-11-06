#!/usr/bin/python3

##############################################################################
#            test_robdd.py
#
#  Copyright  2026  Luca Geretti
##############################################################################

from pyariadne_bdd import RobddManager, BinaryWord


def test_robdd_smoke():
    manager = RobddManager()

    x = manager.variable(0)
    y = manager.variable(1)

    assert manager.variable(0) == x
    assert manager.conjunction(x, y) == manager.conjunction(y, x)

    xor_xy = manager.exclusive_or(x, y)

    assert not manager.evaluate(xor_xy, BinaryWord("00"))
    assert manager.evaluate(xor_xy, BinaryWord("01"))
    assert manager.evaluate(xor_xy, BinaryWord("10"))
    assert not manager.evaluate(xor_xy, BinaryWord("11"))

def test_robdd_structural_inspection():
    import pytest
    from pyariadne_bdd import BddVariable

    manager = RobddManager()
    false = manager.constant(False)
    true = manager.constant(True)
    z = manager.variable(3)
    root = manager.make_node(BddVariable(0), false, z)

    assert manager.top_variable(false) is None
    assert manager.top_variable(root).index() == 0
    assert manager.top_variable(manager.high(root)).index() == 3
    assert manager.low(root) == false
    assert manager.high(root) == z

    negated = manager.negation(root)
    before = manager.node_count()
    assert manager.low(negated) == true
    assert manager.high(negated) == manager.negation(z)
    assert manager.node_count() == before
    assert manager.make_node(BddVariable(0), manager.low(negated), manager.high(negated)) == negated
    assert manager.make_node(BddVariable(0), z, z) == z

    with pytest.raises(Exception):
        manager.low(false)
    with pytest.raises(Exception):
        manager.high(true)
    with pytest.raises(Exception):
        manager.make_node(BddVariable(3), false, z)
    with pytest.raises(Exception):
        manager.make_node(BddVariable(4), z, false)

    other = RobddManager()
    with pytest.raises(Exception):
        manager.make_node(BddVariable(0), false, other.variable(4))

def test_robdd_support_shift_rename():
    import pytest

    manager = RobddManager()
    x = manager.variable(1)
    z = manager.variable(4)
    formula = manager.conjunction(x, manager.negation(z))

    assert [variable.index() for variable in manager.support(formula)] == [1, 4]
    assert [variable.index() for variable in manager.support(manager.negation(formula))] == [1, 4]
    assert manager.support(manager.constant(False)) == []
    assert manager.shift(formula, 0) == formula
    assert manager.shift(formula, 2) == manager.conjunction(
        manager.variable(3), manager.negation(manager.variable(6)))
    assert manager.shift(formula, -1) == manager.conjunction(
        manager.variable(0), manager.negation(manager.variable(3)))
    with pytest.raises(Exception):
        manager.shift(formula, -2)

    swapped = manager.rename(formula, {1: 4, 4: 1})
    assert swapped == manager.conjunction(z, manager.negation(x))
    assert manager.rename(formula, {1: 4}).is_false()
    assert manager.rename(formula, {1: 5, 4: 2}) == manager.conjunction(
        manager.variable(5), manager.negation(manager.variable(2)))
    assert manager.rename(manager.negation(formula), {1: 4, 4: 1}) == manager.negation(swapped)

    other = RobddManager()
    with pytest.raises(Exception):
        manager.rename(other.variable(1), {1: 3})
