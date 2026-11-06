/***************************************************************************
 *            robdd.hpp
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

#ifndef ARIADNE_BDD_ROBDD_HPP
#define ARIADNE_BDD_ROBDD_HPP

#include <map>
#include <mutex>
#include <tuple>
#include <utility>
#include <vector>

#include "utility/typedefs.hpp"
#include "utility/binary_word.hpp"
#include "bdd/variable.hpp"

namespace Ariadne {

class RobddManager;

//! \brief Lightweight handle to a reduced ordered binary decision diagram.
//! \details The root is a semantic node reference whose low bit records
//! complement polarity. The owning manager must outlive every handle.
class Robdd {
  public:
    Bool is_false() const { return _root == 0u; }
    Bool is_true() const { return _root == 1u; }
    Bool is_terminal() const { return _root <= 1u; }

    friend Bool operator==(Robdd const& lhs, Robdd const& rhs) {
        return lhs._manager == rhs._manager and lhs._root == rhs._root;
    }
    friend Bool operator!=(Robdd const& lhs, Robdd const& rhs) { return not (lhs == rhs); }

  private:
    using NodeRef = SizeType;

    Robdd(RobddManager* manager, NodeRef root) : _manager(manager), _root(root) { }

    RobddManager* _manager;
    NodeRef _root;

    friend class RobddManager;
};

//! \brief Manager owning canonical ROBDD nodes and the ITE computed table.
//! \details Complemented edges are the single internal representation.
//! Physical nodes are canonicalized with a regular low reference.
class RobddManager {
  public:
    RobddManager() = default;
    RobddManager(RobddManager const&) = delete;
    RobddManager& operator=(RobddManager const&) = delete;

    Robdd constant(Bool value);

    Robdd variable(SizeType index);
    Robdd variable(BddVariable variable);

    Robdd negation(Robdd const& value);
    Robdd conjunction(Robdd const& lhs, Robdd const& rhs);
    Robdd disjunction(Robdd const& lhs, Robdd const& rhs);
    Robdd exclusive_or(Robdd const& lhs, Robdd const& rhs);
    Robdd ite(Robdd const& condition, Robdd const& then_branch, Robdd const& else_branch);

    Bool evaluate(Robdd const& value, BinaryWord const& assignment) const;

    //! \brief Number of unique physical non-terminal nodes owned by the manager.
    SizeType node_count() const;

  private:
    using NodeRef = Robdd::NodeRef;
    using NodeKey = std::tuple<SizeType,NodeRef,NodeRef>;
    using IteKey = std::tuple<NodeRef,NodeRef,NodeRef>;

    struct Node {
        BddVariable variable;
        NodeRef low;
        NodeRef high;
    };

    static constexpr NodeRef FALSE_NODE = 0u;
    static constexpr NodeRef TRUE_NODE = 1u;

    static Bool _is_terminal(NodeRef ref) { return ref <= TRUE_NODE; }
    static Bool _is_complemented(NodeRef ref) { return (ref & 1u) != 0u; }
    static NodeRef _negate(NodeRef ref) { return ref ^ 1u; }
    static SizeType _node_index(NodeRef ref) { return (ref >> 1u) - 1u; }
    static NodeRef _make_regular_node_ref(SizeType index) { return (index + 1u) << 1u; }

    NodeRef _validate(Robdd const& value) const;
    Node _node(NodeRef ref) const;
    SizeType _top_variable(NodeRef ref) const;
    std::pair<NodeRef,NodeRef> _cofactor(NodeRef ref, SizeType variable) const;

    NodeRef _make_node(BddVariable variable, NodeRef low, NodeRef high);
    NodeRef _ite(NodeRef condition, NodeRef then_branch, NodeRef else_branch);

  private:
    mutable std::mutex _mutex;
    std::vector<Node> _nodes;
    std::map<NodeKey,NodeRef> _unique_table;
    std::map<IteKey,NodeRef> _ite_cache;
};

} // namespace Ariadne

#endif // ARIADNE_BDD_ROBDD_HPP
