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

def test_robdd_restrict_exists_and_exists():
    import pytest
    from pyariadne_bdd import BddVariable

    manager = RobddManager()
    x = manager.variable(0)
    y = manager.variable(2)
    z = manager.variable(4)
    formula = manager.conjunction(x, manager.exclusive_or(y, z))
    middle = BddVariable(2)

    assert manager.restrict(formula, middle, True) == manager.conjunction(
        x, manager.negation(z))
    assert manager.restrict(formula, middle, False) == manager.conjunction(x, z)
    assert manager.restrict(formula, BddVariable(1), True) == formula
    assert manager.restrict(manager.constant(True), middle, False).is_true()

    assert manager.exists(formula, [middle]) == x
    assert manager.exists(formula, [middle, middle]) == x
    assert manager.exists(formula, [BddVariable(1)]) == formula
    assert manager.exists(formula, []) == formula

    assert manager.exists(x, [BddVariable(0)]).is_true()
    assert manager.exists(manager.negation(x), [BddVariable(0)]).is_true()

    assert manager.and_exists(formula, manager.negation(y), [middle]) == (
        manager.conjunction(x, z))
    assert manager.and_exists(formula, manager.constant(True), [middle]) == x
    assert manager.and_exists(formula, formula, [middle]) == x
    assert manager.and_exists(formula, manager.negation(formula), [middle]).is_false()
    assert manager.and_exists(formula, manager.constant(True), []) == formula

    other = RobddManager()
    with pytest.raises(Exception):
        manager.restrict(other.variable(0), BddVariable(0), False)
    with pytest.raises(Exception):
        manager.exists(other.variable(0), [BddVariable(0)])
    with pytest.raises(Exception):
        manager.and_exists(formula, other.variable(0), [middle])

def test_robdd_enabled_cells_depth():
    import pytest

    manager = RobddManager()
    false = manager.constant(False)
    true = manager.constant(True)
    x = manager.variable(0)
    z = manager.variable(2)
    formula = manager.conjunction(x,manager.negation(z))

    assert manager.enabled_cell_count(false, 5) == 0
    assert manager.enabled_cell_count(true, 0) == 1
    assert manager.enabled_cell_count(true, 4) == 16
    assert manager.enabled_cell_count(formula, 3) == 2
    assert manager.enabled_cell_count(formula, 5) == 8
    assert manager.enabled_cell_count(manager.negation(formula), 3) == 6
    assert manager.enabled_cells(true, 0, 1)[0].__len__() == 0
    assert manager.enabled_cells(false, 8, 0) == []
    assert [str(cell) for cell in manager.enabled_cells(formula, 3, 2)] == [
        "100", "110"
    ]
    assert [str(cell) for cell in manager.enabled_cells(formula, 4, 4)] == [
        "1000", "1001", "1100", "1101"
    ]
    shifted = manager.shift(formula, 4)
    assert [str(cell) for cell in manager.enabled_cells(shifted, 3, 2, 4)] == [
        "100", "110"
    ]

    with pytest.raises(Exception):
        manager.enabled_cells(formula, 3, 1)
    with pytest.raises(Exception):
        manager.enabled_cell_count(formula, 2)
    with pytest.raises(Exception):
        manager.enabled_cells(formula, 2, 8)
    with pytest.raises(Exception):
        manager.enabled_cell_count(manager.variable(0), 1, 1)
    with pytest.raises(Exception):
        manager.enabled_cell_count(true, 64)

    other = RobddManager()
    with pytest.raises(Exception):
        manager.enabled_cell_count(other.constant(True), 0)
