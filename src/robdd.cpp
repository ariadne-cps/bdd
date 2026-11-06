/***************************************************************************
 *            robdd.cpp
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

#include <algorithm>
#include <limits>

#include "utility/macros.hpp"
#include "bdd/robdd.hpp"

namespace Ariadne {

Robdd RobddManager::constant(Bool value) {
    return Robdd(this,value ? TRUE_NODE : FALSE_NODE);
}

Robdd RobddManager::variable(SizeType index) {
    return variable(BddVariable(index));
}

Robdd RobddManager::variable(BddVariable variable) {
    return Robdd(this,_make_node(variable,FALSE_NODE,TRUE_NODE));
}

Robdd RobddManager::negation(Robdd const& value) {
    return Robdd(this,_negate(_validate(value)));
}

Robdd RobddManager::conjunction(Robdd const& lhs, Robdd const& rhs) {
    NodeRef l = _validate(lhs);
    NodeRef r = _validate(rhs);
    return Robdd(this,_ite(l,r,FALSE_NODE));
}

Robdd RobddManager::disjunction(Robdd const& lhs, Robdd const& rhs) {
    NodeRef l = _validate(lhs);
    NodeRef r = _validate(rhs);
    return Robdd(this,_ite(l,TRUE_NODE,r));
}

Robdd RobddManager::exclusive_or(Robdd const& lhs, Robdd const& rhs) {
    NodeRef l = _validate(lhs);
    NodeRef r = _validate(rhs);
    return Robdd(this,_ite(l,_negate(r),r));
}

Robdd RobddManager::ite(
        Robdd const& condition,
        Robdd const& then_branch,
        Robdd const& else_branch)
{
    NodeRef c = _validate(condition);
    NodeRef t = _validate(then_branch);
    NodeRef e = _validate(else_branch);
    return Robdd(this,_ite(c,t,e));
}

Bool RobddManager::evaluate(
        Robdd const& value,
        BinaryWord const& assignment) const
{
    NodeRef current = _validate(value);
    while (not _is_terminal(current)) {
        Node node = _node(current);
        ARIADNE_PRECONDITION_MSG(node.variable.index() < assignment.size(),
            "Missing assignment for BDD variable " << node.variable.index());

        NodeRef next = assignment[node.variable.index()] ? node.high : node.low;
        if (_is_complemented(current)) {
            next = _negate(next);
        }
        current = next;
    }
    return current == TRUE_NODE;
}

SizeType RobddManager::node_count() const {
    std::lock_guard<std::mutex> lock(_mutex);
    return _nodes.size();
}

RobddManager::NodeRef RobddManager::_validate(Robdd const& value) const {
    ARIADNE_PRECONDITION_MSG(value._manager == this,
        "ROBDD operands must belong to the same manager");
    return value._root;
}

RobddManager::Node RobddManager::_node(NodeRef ref) const {
    std::lock_guard<std::mutex> lock(_mutex);
    return _nodes[_node_index(ref)];
}

SizeType RobddManager::_top_variable(NodeRef ref) const {
    if (_is_terminal(ref)) {
        return std::numeric_limits<SizeType>::max();
    }
    return _node(ref).variable.index();
}

std::pair<RobddManager::NodeRef,RobddManager::NodeRef>
RobddManager::_cofactor(NodeRef ref, SizeType variable) const {
    if (_is_terminal(ref)) {
        return {ref,ref};
    }

    Node node = _node(ref);
    if (node.variable.index() != variable) {
        return {ref,ref};
    }

    NodeRef low = node.low;
    NodeRef high = node.high;
    if (_is_complemented(ref)) {
        low = _negate(low);
        high = _negate(high);
    }
    return {low,high};
}

RobddManager::NodeRef
RobddManager::_make_node(BddVariable variable, NodeRef low, NodeRef high) {
    if (low == high) {
        return low;
    }

    // Canonical complemented-edge normalization: every physical node has a
    // regular low reference. If low is complemented, complement both children
    // and carry the complement on the reference returned to the caller.
    Bool complement_result = false;
    if (_is_complemented(low)) {
        low = _negate(low);
        high = _negate(high);
        complement_result = true;
    }

    NodeKey key(variable.index(),low,high);
    NodeRef result;

    {
        std::lock_guard<std::mutex> lock(_mutex);

        auto found = _unique_table.find(key);
        if (found != _unique_table.end()) {
            result = found->second;
        } else {
            result = _make_regular_node_ref(_nodes.size());
            _nodes.push_back(Node{variable,low,high});
            _unique_table.emplace(key,result);
        }
    }

    return complement_result ? _negate(result) : result;
}

RobddManager::NodeRef
RobddManager::_ite(NodeRef condition, NodeRef then_branch, NodeRef else_branch) {
    if (condition == TRUE_NODE) {
        return then_branch;
    }
    if (condition == FALSE_NODE) {
        return else_branch;
    }
    if (then_branch == else_branch) {
        return then_branch;
    }
    if (then_branch == TRUE_NODE and else_branch == FALSE_NODE) {
        return condition;
    }
    if (then_branch == FALSE_NODE and else_branch == TRUE_NODE) {
        return _negate(condition);
    }

    IteKey key(condition,then_branch,else_branch);
    {
        std::lock_guard<std::mutex> lock(_mutex);
        auto found = _ite_cache.find(key);
        if (found != _ite_cache.end()) {
            return found->second;
        }
    }

    SizeType variable = std::min(
        _top_variable(condition),
        std::min(_top_variable(then_branch),_top_variable(else_branch)));

    auto condition_cofactors = _cofactor(condition,variable);
    auto then_cofactors = _cofactor(then_branch,variable);
    auto else_cofactors = _cofactor(else_branch,variable);

    NodeRef low = _ite(
        condition_cofactors.first,
        then_cofactors.first,
        else_cofactors.first);
    NodeRef high = _ite(
        condition_cofactors.second,
        then_cofactors.second,
        else_cofactors.second);

    NodeRef result = _make_node(BddVariable(variable),low,high);

    {
        std::lock_guard<std::mutex> lock(_mutex);
        _ite_cache.emplace(key,result);
    }
    return result;
}

} // namespace Ariadne
