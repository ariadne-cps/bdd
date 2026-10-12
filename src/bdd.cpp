/***************************************************************************
 *            bdd.cpp
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

#include "macro/general.hpp"
#include "bdd/bdd.hpp"

namespace Ariadne {

Bdd BddManager::constant(Bool value) {
    return Bdd(this,value ? TRUE_NODE : FALSE_NODE);
}

Bdd BddManager::variable(SizeType index) {
    return variable(BddVariable(index));
}

Bdd BddManager::variable(BddVariable variable) {
    return Bdd(this,_make_node(variable,FALSE_NODE,TRUE_NODE));
}

Bdd BddManager::negation(Bdd const& value) {
    NodeId root = _validate(value);
    return Bdd(this,_ite(root,FALSE_NODE,TRUE_NODE));
}

Bdd BddManager::conjunction(Bdd const& lhs, Bdd const& rhs) {
    NodeId l = _validate(lhs);
    NodeId r = _validate(rhs);
    return Bdd(this,_ite(l,r,FALSE_NODE));
}

Bdd BddManager::disjunction(Bdd const& lhs, Bdd const& rhs) {
    NodeId l = _validate(lhs);
    NodeId r = _validate(rhs);
    return Bdd(this,_ite(l,TRUE_NODE,r));
}

Bdd BddManager::exclusive_or(Bdd const& lhs, Bdd const& rhs) {
    NodeId l = _validate(lhs);
    NodeId r = _validate(rhs);
    NodeId not_r = _ite(r,FALSE_NODE,TRUE_NODE);
    return Bdd(this,_ite(l,not_r,r));
}

Bdd BddManager::ite(Bdd const& condition, Bdd const& then_branch, Bdd const& else_branch) {
    NodeId c = _validate(condition);
    NodeId t = _validate(then_branch);
    NodeId e = _validate(else_branch);
    return Bdd(this,_ite(c,t,e));
}

Bool BddManager::evaluate(Bdd const& value, BinaryWord const& assignment) const {
    NodeId current = _validate(value);
    while (current != FALSE_NODE and current != TRUE_NODE) {
        Node node = _node(current);
        ARIADNE_PRECONDITION_MSG(node.variable.index() < assignment.size(),
            "Missing assignment for BDD variable " << node.variable.index());
        current = assignment[node.variable.index()] ? node.high : node.low;
    }
    return current == TRUE_NODE;
}

SizeType BddManager::node_count() const {
    std::lock_guard<std::mutex> lock(_mutex);
    return _nodes.size();
}

BddManager::NodeId BddManager::_validate(Bdd const& value) const {
    ARIADNE_PRECONDITION_MSG(value._manager == this,
        "BDD operands must belong to the same manager");
    return value._root;
}

BddManager::Node BddManager::_node(NodeId id) const {
    std::lock_guard<std::mutex> lock(_mutex);
    SizeType index = id - 2u;
    return _nodes[index];
}

SizeType BddManager::_top_variable(NodeId id) const {
    if (id == FALSE_NODE or id == TRUE_NODE) {
        return std::numeric_limits<SizeType>::max();
    }
    return _node(id).variable.index();
}

std::pair<BddManager::NodeId,BddManager::NodeId>
BddManager::_cofactor(NodeId id, SizeType variable) const {
    if (id == FALSE_NODE or id == TRUE_NODE) {
        return {id,id};
    }
    Node node = _node(id);
    if (node.variable.index() == variable) {
        return {node.low,node.high};
    }
    return {id,id};
}

BddManager::NodeId
BddManager::_make_node(BddVariable variable, NodeId low, NodeId high) {
    if (low == high) {
        return low;
    }

    NodeKey key(variable.index(),low,high);
    std::lock_guard<std::mutex> lock(_mutex);

    auto found = _unique_table.find(key);
    if (found != _unique_table.end()) {
        return found->second;
    }

    NodeId id = _nodes.size() + 2u;
    _nodes.push_back(Node{variable,low,high});
    _unique_table.emplace(key,id);
    return id;
}

BddManager::NodeId
BddManager::_ite(NodeId condition, NodeId then_branch, NodeId else_branch) {
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

    NodeId low = _ite(
        condition_cofactors.first,
        then_cofactors.first,
        else_cofactors.first);
    NodeId high = _ite(
        condition_cofactors.second,
        then_cofactors.second,
        else_cofactors.second);

    NodeId result = _make_node(BddVariable(variable),low,high);

    {
        std::lock_guard<std::mutex> lock(_mutex);
        _ite_cache.emplace(key,result);
    }
    return result;
}

} // namespace Ariadne
