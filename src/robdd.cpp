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
#include "bdd/detail/robdd_impl.hpp"

namespace Ariadne {
namespace Detail {

template<class EdgePolicy>
typename BasicRobddManager<EdgePolicy>::Diagram
BasicRobddManager<EdgePolicy>::constant(Bool value) {
    return Diagram(this,value ? TRUE_NODE : FALSE_NODE);
}

template<class EdgePolicy>
typename BasicRobddManager<EdgePolicy>::Diagram
BasicRobddManager<EdgePolicy>::variable(SizeType index) {
    return variable(BddVariable(index));
}

template<class EdgePolicy>
typename BasicRobddManager<EdgePolicy>::Diagram
BasicRobddManager<EdgePolicy>::variable(BddVariable variable) {
    return Diagram(this,_make_node(variable,FALSE_NODE,TRUE_NODE));
}

template<class EdgePolicy>
typename BasicRobddManager<EdgePolicy>::Diagram
BasicRobddManager<EdgePolicy>::negation(Diagram const& value) {
    NodeRef root = _validate(value);
    if constexpr (EdgePolicy::complemented_edges) {
        return Diagram(this,EdgePolicy::negate(root));
    } else {
        return Diagram(this,_ite(root,FALSE_NODE,TRUE_NODE));
    }
}

template<class EdgePolicy>
typename BasicRobddManager<EdgePolicy>::Diagram
BasicRobddManager<EdgePolicy>::conjunction(Diagram const& lhs, Diagram const& rhs) {
    NodeRef l = _validate(lhs);
    NodeRef r = _validate(rhs);
    return Diagram(this,_ite(l,r,FALSE_NODE));
}

template<class EdgePolicy>
typename BasicRobddManager<EdgePolicy>::Diagram
BasicRobddManager<EdgePolicy>::disjunction(Diagram const& lhs, Diagram const& rhs) {
    NodeRef l = _validate(lhs);
    NodeRef r = _validate(rhs);
    return Diagram(this,_ite(l,TRUE_NODE,r));
}

template<class EdgePolicy>
typename BasicRobddManager<EdgePolicy>::Diagram
BasicRobddManager<EdgePolicy>::exclusive_or(Diagram const& lhs, Diagram const& rhs) {
    NodeRef l = _validate(lhs);
    NodeRef r = _validate(rhs);
    NodeRef not_r;
    if constexpr (EdgePolicy::complemented_edges) {
        not_r = EdgePolicy::negate(r);
    } else {
        not_r = _ite(r,FALSE_NODE,TRUE_NODE);
    }
    return Diagram(this,_ite(l,not_r,r));
}

template<class EdgePolicy>
typename BasicRobddManager<EdgePolicy>::Diagram
BasicRobddManager<EdgePolicy>::ite(
        Diagram const& condition,
        Diagram const& then_branch,
        Diagram const& else_branch)
{
    NodeRef c = _validate(condition);
    NodeRef t = _validate(then_branch);
    NodeRef e = _validate(else_branch);
    return Diagram(this,_ite(c,t,e));
}

template<class EdgePolicy>
Bool BasicRobddManager<EdgePolicy>::evaluate(
        Diagram const& value,
        BinaryWord const& assignment) const
{
    NodeRef current = _validate(value);
    while (not EdgePolicy::is_terminal(current)) {
        Node node = _node(current);
        ARIADNE_PRECONDITION_MSG(node.variable.index() < assignment.size(),
            "Missing assignment for BDD variable " << node.variable.index());

        NodeRef next = assignment[node.variable.index()] ? node.high : node.low;
        if constexpr (EdgePolicy::complemented_edges) {
            if (EdgePolicy::is_complemented(current)) {
                next = EdgePolicy::negate(next);
            }
        }
        current = next;
    }
    return current == TRUE_NODE;
}

template<class EdgePolicy>
SizeType BasicRobddManager<EdgePolicy>::node_count() const {
    std::lock_guard<std::mutex> lock(_mutex);
    return _nodes.size();
}

template<class EdgePolicy>
typename BasicRobddManager<EdgePolicy>::NodeRef
BasicRobddManager<EdgePolicy>::_validate(Diagram const& value) const {
    ARIADNE_PRECONDITION_MSG(value._manager == this,
        "ROBDD operands must belong to the same manager");
    return value._root;
}

template<class EdgePolicy>
typename BasicRobddManager<EdgePolicy>::Node
BasicRobddManager<EdgePolicy>::_node(NodeRef ref) const {
    std::lock_guard<std::mutex> lock(_mutex);
    return _nodes[EdgePolicy::node_index(ref)];
}

template<class EdgePolicy>
SizeType BasicRobddManager<EdgePolicy>::_top_variable(NodeRef ref) const {
    if (EdgePolicy::is_terminal(ref)) {
        return std::numeric_limits<SizeType>::max();
    }
    return _node(ref).variable.index();
}

template<class EdgePolicy>
std::pair<typename BasicRobddManager<EdgePolicy>::NodeRef,
          typename BasicRobddManager<EdgePolicy>::NodeRef>
BasicRobddManager<EdgePolicy>::_cofactor(NodeRef ref, SizeType variable) const {
    if (EdgePolicy::is_terminal(ref)) {
        return {ref,ref};
    }

    Node node = _node(ref);
    if (node.variable.index() != variable) {
        return {ref,ref};
    }

    NodeRef low = node.low;
    NodeRef high = node.high;
    if constexpr (EdgePolicy::complemented_edges) {
        if (EdgePolicy::is_complemented(ref)) {
            low = EdgePolicy::negate(low);
            high = EdgePolicy::negate(high);
        }
    }
    return {low,high};
}

template<class EdgePolicy>
typename BasicRobddManager<EdgePolicy>::NodeRef
BasicRobddManager<EdgePolicy>::_make_node(
        BddVariable variable,
        NodeRef low,
        NodeRef high)
{
    if (low == high) {
        return low;
    }

    Bool complement_result = false;
    if constexpr (EdgePolicy::complemented_edges) {
        // Canonical complemented-edge normalization: every physical node has
        // a regular low edge. Negating both children preserves the decision
        // node up to a complemented reference to its result.
        if (EdgePolicy::is_complemented(low)) {
            low = EdgePolicy::negate(low);
            high = EdgePolicy::negate(high);
            complement_result = true;
        }
    }

    NodeKey key(variable.index(),low,high);
    NodeRef result;

    {
        std::lock_guard<std::mutex> lock(_mutex);

        auto found = _unique_table.find(key);
        if (found != _unique_table.end()) {
            result = found->second;
        } else {
            result = EdgePolicy::make_regular_node_ref(_nodes.size());
            _nodes.push_back(Node{variable,low,high});
            _unique_table.emplace(key,result);
        }
    }

    if constexpr (EdgePolicy::complemented_edges) {
        if (complement_result) {
            result = EdgePolicy::negate(result);
        }
    }
    return result;
}

template<class EdgePolicy>
typename BasicRobddManager<EdgePolicy>::NodeRef
BasicRobddManager<EdgePolicy>::_ite(
        NodeRef condition,
        NodeRef then_branch,
        NodeRef else_branch)
{
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
    if constexpr (EdgePolicy::complemented_edges) {
        if (then_branch == FALSE_NODE and else_branch == TRUE_NODE) {
            return EdgePolicy::negate(condition);
        }
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

template class BasicRobddManager<RegularEdgePolicy>;
template class BasicRobddManager<ComplementedEdgePolicy>;

} // namespace Detail
} // namespace Ariadne
