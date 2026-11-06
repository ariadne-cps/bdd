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
