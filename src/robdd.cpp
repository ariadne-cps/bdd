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
#include <functional>
#include <limits>
#include <set>

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
    ARIADNE_PRECONDITION_MSG(variable.index() < std::numeric_limits<SizeType>::max(),
        "Maximum variable index is reserved for terminal ordering");
    return Robdd(this,_make_node(variable,FALSE_NODE,TRUE_NODE));
}

std::optional<BddVariable> RobddManager::top_variable(Robdd const& value) const {
    NodeRef root = _validate(value);
    if (_is_terminal(root)) {
        return std::nullopt;
    }
    return _node(root).variable;
}

Robdd RobddManager::low(Robdd const& value) const {
    NodeRef root = _validate(value);
    ARIADNE_PRECONDITION_MSG(not _is_terminal(root),
        "Cannot inspect the low branch of a terminal ROBDD");
    Node node = _node(root);
    return Robdd(const_cast<RobddManager*>(this),
        _is_complemented(root) ? _negate(node.low) : node.low);
}

Robdd RobddManager::high(Robdd const& value) const {
    NodeRef root = _validate(value);
    ARIADNE_PRECONDITION_MSG(not _is_terminal(root),
        "Cannot inspect the high branch of a terminal ROBDD");
    Node node = _node(root);
    return Robdd(const_cast<RobddManager*>(this),
        _is_complemented(root) ? _negate(node.high) : node.high);
}

Robdd RobddManager::make_node(
        BddVariable variable, Robdd const& low, Robdd const& high) {
    NodeRef low_ref = _validate(low);
    NodeRef high_ref = _validate(high);
    ARIADNE_PRECONDITION_MSG(variable.index() < std::numeric_limits<SizeType>::max(),
        "Maximum variable index is reserved for terminal ordering");
    ARIADNE_PRECONDITION_MSG(
        variable.index() < _top_variable(low_ref) and
        variable.index() < _top_variable(high_ref),
        "ROBDD node variable must precede the variables of both children");
    return Robdd(this,_make_node(variable,low_ref,high_ref));
}

std::vector<BddVariable> RobddManager::support(Robdd const& value) const {
    NodeRef root = _validate(value);
    std::set<NodeRef> visited;
    std::set<SizeType> indices;

    std::function<void(NodeRef)> visit = [&](NodeRef ref) {
        if (_is_terminal(ref)) {
            return;
        }
        NodeRef regular = ref & ~NodeRef(1u);
        if (not visited.insert(regular).second) {
            return;
        }
        Node node = _node(regular);
        indices.insert(node.variable.index());
        visit(node.low);
        visit(node.high);
    };
    visit(root);

    std::vector<BddVariable> result;
    result.reserve(indices.size());
    for (SizeType index : indices) {
        result.emplace_back(index);
    }
    return result;
}

Robdd RobddManager::shift(Robdd const& value, std::ptrdiff_t offset) {
    NodeRef root = _validate(value);
    if (_is_terminal(root) or offset == 0) {
        return Robdd(this,root);
    }

    constexpr SizeType max_index = std::numeric_limits<SizeType>::max();
    Bool negative = offset < 0;
    // Avoid negating PTRDIFF_MIN, which cannot be represented as positive
    // ptrdiff_t. SizeType is the unsigned representation of variable indices.
    SizeType magnitude = negative
        ? static_cast<SizeType>(-(offset + 1)) + 1u
        : static_cast<SizeType>(offset);

    ARIADNE_PRECONDITION_MSG(negative or magnitude < max_index,
        "ROBDD variable shift overflows the variable index range");
    // Preflight every supported variable before creating any new nodes.
    for (BddVariable variable : support(value)) {
        SizeType index = variable.index();
        ARIADNE_PRECONDITION_MSG(
            negative ? index >= magnitude : index < max_index - magnitude,
            "ROBDD variable shift leaves the valid variable index range");
    }

    std::map<NodeRef,NodeRef> memo;
    std::function<NodeRef(NodeRef)> rebuild = [&](NodeRef ref) -> NodeRef {
        if (_is_terminal(ref)) {
            return ref;
        }
        NodeRef regular = ref & ~NodeRef(1u);
        auto found = memo.find(regular);
        if (found == memo.end()) {
            Node node = _node(regular);
            NodeRef low = rebuild(node.low);
            NodeRef high = rebuild(node.high);
            SizeType index = node.variable.index();
            SizeType shifted_index = negative ? index - magnitude : index + magnitude;
            NodeRef shifted = _make_node(BddVariable(shifted_index),low,high);
            found = memo.emplace(regular,shifted).first;
        }
        return _is_complemented(ref) ? _negate(found->second) : found->second;
    };
    return Robdd(this,rebuild(root));
}

Robdd RobddManager::rename(
        Robdd const& value, std::map<SizeType,SizeType> const& indices) {
    NodeRef root = _validate(value);
    constexpr SizeType max_index = std::numeric_limits<SizeType>::max();
    for (auto const& entry : indices) {
        ARIADNE_PRECONDITION_MSG(entry.first < max_index and entry.second < max_index,
            "ROBDD variable renaming uses the reserved terminal-order index");
    }
    if (_is_terminal(root) or indices.empty()) {
        return Robdd(this,root);
    }

    std::map<NodeRef,NodeRef> memo;
    std::function<NodeRef(NodeRef)> rebuild = [&](NodeRef ref) -> NodeRef {
        if (_is_terminal(ref)) {
            return ref;
        }
        NodeRef regular = ref & ~NodeRef(1u);
        auto found = memo.find(regular);
        if (found == memo.end()) {
            Node node = _node(regular);
            NodeRef low = rebuild(node.low);
            NodeRef high = rebuild(node.high);
            auto replacement = indices.find(node.variable.index());
            SizeType destination = replacement == indices.end()
                ? node.variable.index() : replacement->second;
            // A general simultaneous substitution may reverse the variable
            // order or identify variables. _make_node alone is not valid here.
            NodeRef variable = _make_node(BddVariable(destination),FALSE_NODE,TRUE_NODE);
            NodeRef renamed = _ite(variable,high,low);
            found = memo.emplace(regular,renamed).first;
        }
        return _is_complemented(ref) ? _negate(found->second) : found->second;
    };
    return Robdd(this,rebuild(root));
}

Robdd RobddManager::restrict(
        Robdd const& value, BddVariable variable, Bool assignment) {
    NodeRef root = _validate(value);
    ARIADNE_PRECONDITION_MSG(
        variable.index() < std::numeric_limits<SizeType>::max(),
        "Cannot restrict the reserved terminal-order variable index");

    std::map<NodeRef,NodeRef> memo;
    std::function<NodeRef(NodeRef)> walk = [&](NodeRef ref) -> NodeRef {
        if (_is_terminal(ref) or _top_variable(ref) > variable.index()) {
            return ref;
        }
        auto found = memo.find(ref);
        if (found != memo.end()) {
            return found->second;
        }

        SizeType top = _top_variable(ref);
        auto branches = _cofactor(ref,top);
        NodeRef result;
        if (top == variable.index()) {
            result = assignment ? branches.second : branches.first;
        } else {
            NodeRef low = walk(branches.first);
            NodeRef high = walk(branches.second);
            result = _make_node(BddVariable(top),low,high);
        }
        memo.emplace(ref,result);
        return result;
    };
    return Robdd(this,walk(root));
}

Robdd RobddManager::exists(
        Robdd const& value, std::vector<BddVariable> const& variables) {
    NodeRef root = _validate(value);
    constexpr SizeType max_index = std::numeric_limits<SizeType>::max();
    std::set<SizeType> quantified;
    for (BddVariable variable : variables) {
        ARIADNE_PRECONDITION_MSG(variable.index() < max_index,
            "Cannot quantify the reserved terminal-order variable index");
        quantified.insert(variable.index());
    }
    if (quantified.empty() or _is_terminal(root)) {
        return Robdd(this,root);
    }

    // Existential quantification does not commute with complemented edges:
    // exists(v, NOT f) is not generally NOT exists(v, f). Memoize semantic
    // references rather than normalizing away their complement bit.
    std::map<NodeRef,NodeRef> memo;
    std::function<NodeRef(NodeRef)> walk = [&](NodeRef ref) -> NodeRef {
        if (_is_terminal(ref)) {
            return ref;
        }
        auto found = memo.find(ref);
        if (found != memo.end()) {
            return found->second;
        }

        SizeType top = _top_variable(ref);
        auto branches = _cofactor(ref,top);
        NodeRef low = walk(branches.first);
        NodeRef high = walk(branches.second);
        NodeRef result = quantified.count(top)
            ? _ite(low,TRUE_NODE,high)
            : _make_node(BddVariable(top),low,high);
        memo.emplace(ref,result);
        return result;
    };
    return Robdd(this,walk(root));
}

Robdd RobddManager::and_exists(
        Robdd const& lhs, Robdd const& rhs,
        std::vector<BddVariable> const& variables) {
    NodeRef left = _validate(lhs);
    NodeRef right = _validate(rhs);
    constexpr SizeType max_index = std::numeric_limits<SizeType>::max();
    std::set<SizeType> quantified;
    for (BddVariable variable : variables) {
        ARIADNE_PRECONDITION_MSG(variable.index() < max_index,
            "Cannot quantify the reserved terminal-order variable index");
        quantified.insert(variable.index());
    }
    if (quantified.empty()) {
        return Robdd(this,_ite(left,right,FALSE_NODE));
    }

    // Fused conjunction and existential abstraction. Each recursive call
    // visits the next variable of either input, including variables skipped
    // by one operand. Memoization is local to the quantification set.
    using AndKey = std::pair<NodeRef,NodeRef>;
    std::map<AndKey,NodeRef> memo;
    std::function<NodeRef(NodeRef,NodeRef)> walk =
        [&](NodeRef a, NodeRef b) -> NodeRef {
            if (a == FALSE_NODE or b == FALSE_NODE or a == _negate(b)) {
                return FALSE_NODE;
            }
            if (a == TRUE_NODE and b == TRUE_NODE) {
                return TRUE_NODE;
            }

            if (a > b) {
                std::swap(a,b);
            }
            AndKey key(a,b);
            auto found = memo.find(key);
            if (found != memo.end()) {
                return found->second;
            }

            SizeType top = std::min(_top_variable(a),_top_variable(b));
            auto a_branches = _cofactor(a,top);
            auto b_branches = _cofactor(b,top);
            NodeRef low = walk(a_branches.first,b_branches.first);
            NodeRef high = walk(a_branches.second,b_branches.second);
            NodeRef result = quantified.count(top)
                ? _ite(low,TRUE_NODE,high)
                : _make_node(BddVariable(top),low,high);
            memo.emplace(key,result);
            return result;
        };
    return Robdd(this,walk(left,right));
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

SizeType RobddManager::enabled_cell_count(
        Robdd const& value, SizeType depth, SizeType first_variable) const {
    NodeRef root = _validate(value);
    SizeType end = _validate_cell_domain(root,depth,first_variable);
    constexpr SizeType max_count = std::numeric_limits<SizeType>::max();
    constexpr SizeType count_bits = std::numeric_limits<SizeType>::digits;

    // Memoize semantic references AND the current subdivision level: the
    // same physical DAG node may be reached after different skipped levels.
    std::map<std::pair<NodeRef,SizeType>,SizeType> memo;
    std::function<SizeType(NodeRef,SizeType)> count =
        [&](NodeRef ref, SizeType level) -> SizeType {
            if (ref == FALSE_NODE) {
                return 0u;
            }
            if (ref == TRUE_NODE) {
                SizeType remaining = end - level;
                ARIADNE_PRECONDITION_MSG(remaining < count_bits,
                    "Enabled-cell count exceeds SizeType capacity");
                return SizeType(1u) << remaining;
            }

            auto key = std::make_pair(ref,level);
            auto found = memo.find(key);
            if (found != memo.end()) {
                return found->second;
            }

            SizeType top = _top_variable(ref);
            auto branches = _cofactor(ref,top);
            SizeType low_count = count(branches.first,top+1u);
            SizeType high_count = count(branches.second,top+1u);
            ARIADNE_PRECONDITION_MSG(low_count <= max_count - high_count,
                "Enabled-cell count exceeds SizeType capacity");
            SizeType subtotal = low_count + high_count;
            SizeType skipped = top - level;
            ARIADNE_PRECONDITION_MSG(
                skipped < count_bits and subtotal <= (max_count >> skipped),
                "Enabled-cell count exceeds SizeType capacity");
            SizeType result = subtotal << skipped;
            memo.emplace(key,result);
            return result;
        };
    return count(root,first_variable);
}

void RobddManager::for_each_enabled_cell(
        Robdd const& value, SizeType depth,
        std::function<void(BinaryWord const&)> const& visitor,
        SizeType first_variable) const {
    NodeRef root = _validate(value);
    SizeType end = _validate_cell_domain(root,depth,first_variable);
    ARIADNE_PRECONDITION_MSG(static_cast<Bool>(visitor),
        "Enabled-cell visitor must be nonempty");

    BinaryWord word;
    std::function<void(NodeRef,SizeType)> traverse =
        [&](NodeRef ref, SizeType level) {
            if (ref == FALSE_NODE) {
                return;
            }
            if (level == end) {
                // Domain validation guarantees there are no remaining nodes.
                visitor(word);
                return;
            }

            NodeRef low = ref;
            NodeRef high = ref;
            if (ref != TRUE_NODE and _top_variable(ref) == level) {
                auto branches = _cofactor(ref,level);
                low = branches.first;
                high = branches.second;
            }
            // For a skipped level or true terminal both choices have
            // identical subfunctions, but correspond to distinct cells.
            word.append(false);
            traverse(low,level+1u);
            word.set_back(true);
            traverse(high,level+1u);
            word.pop_back();
        };
    traverse(root,first_variable);
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

SizeType RobddManager::_validate_cell_domain(
        NodeRef root, SizeType depth, SizeType first_variable) const {
    constexpr SizeType max_index = std::numeric_limits<SizeType>::max();
    ARIADNE_PRECONDITION_MSG(
        first_variable < max_index and depth <= max_index - first_variable,
        "Enabled-cell subdivision range exceeds valid variable indices");
    SizeType end = first_variable + depth;

    std::set<NodeRef> visited;
    std::function<void(NodeRef)> check = [&](NodeRef ref) {
        if (_is_terminal(ref)) {
            return;
        }
        NodeRef regular = ref & ~NodeRef(1u);
        if (not visited.insert(regular).second) {
            return;
        }
        Node node = _node(regular);
        ARIADNE_PRECONDITION_MSG(
            node.variable.index() >= first_variable and node.variable.index() < end,
            "ROBDD support lies outside the finite subdivision domain");
        check(node.low);
        check(node.high);
    };
    check(root);
    return end;
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
