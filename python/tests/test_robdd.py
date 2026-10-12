#!/usr/bin/python3

##############################################################################
#            test_robdd.py
#
#  Copyright  2026  Luca Geretti
##############################################################################

from pyariadne import RobddManager, BinaryWord


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
