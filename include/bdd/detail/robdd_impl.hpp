/***************************************************************************
 *            robdd_impl.hpp
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

#ifndef ARIADNE_BDD_DETAIL_ROBDD_IMPL_HPP
#define ARIADNE_BDD_DETAIL_ROBDD_IMPL_HPP

#include <map>
#include <mutex>
#include <tuple>
#include <utility>
#include <vector>

#include "utility/typedefs.hpp"
#include "utility/binary_word.hpp"
#include "bdd/variable.hpp"

namespace Ariadne {
namespace Detail {

struct RegularEdgePolicy {
    using NodeRef = SizeType;

    static constexpr Bool complemented_edges = false;
    static constexpr NodeRef FALSE_NODE = 0u;
    static constexpr NodeRef TRUE_NODE = 1u;

    static Bool is_terminal(NodeRef ref) { return ref <= TRUE_NODE; }
    static SizeType node_index(NodeRef ref) { return ref - 2u; }
    static NodeRef make_regular_node_ref(SizeType index) { return index + 2u; }
    static Bool is_complemented(NodeRef) { return false; }
};

struct ComplementedEdgePolicy {
    using NodeRef = SizeType;

    static constexpr Bool complemented_edges = true;
    static constexpr NodeRef FALSE_NODE = 0u;
    static constexpr NodeRef TRUE_NODE = 1u;

    static Bool is_terminal(NodeRef ref) { return (ref >> 1u) == 0u; }
    static SizeType node_index(NodeRef ref) { return (ref >> 1u) - 1u; }
    static NodeRef make_regular_node_ref(SizeType index) { return (index + 1u) << 1u; }
    static Bool is_complemented(NodeRef ref) { return (ref & 1u) != 0u; }
    static NodeRef negate(NodeRef ref) { return ref ^ 1u; }
};

template<class EdgePolicy> class BasicRobddManager;

//! \brief Lightweight handle to a reduced ordered binary decision diagram.
template<class EdgePolicy>
class BasicRobdd {
  public:
    Bool is_false() const { return _root == EdgePolicy::FALSE_NODE; }
    Bool is_true() const { return _root == EdgePolicy::TRUE_NODE; }
    Bool is_terminal() const { return EdgePolicy::is_terminal(_root); }

    friend Bool operator==(BasicRobdd const& lhs, BasicRobdd const& rhs) {
        return lhs._manager == rhs._manager and lhs._root == rhs._root;
    }
    friend Bool operator!=(BasicRobdd const& lhs, BasicRobdd const& rhs) { return not (lhs == rhs); }

  private:
    using NodeRef = typename EdgePolicy::NodeRef;

    BasicRobdd(BasicRobddManager<EdgePolicy>* manager, NodeRef root)
        : _manager(manager), _root(root) { }

    BasicRobddManager<EdgePolicy>* _manager;
    NodeRef _root;

    friend class BasicRobddManager<EdgePolicy>;
};

//! \brief ROBDD manager specialized at compile time for an edge representation.
//! \details The public ROBDD manager uses complemented edges. The regular-edge
//! specialization is retained only as an internal benchmark/regression baseline.
template<class EdgePolicy>
class BasicRobddManager {
  public:
    using Diagram = BasicRobdd<EdgePolicy>;

    BasicRobddManager() = default;
    BasicRobddManager(BasicRobddManager const&) = delete;
    BasicRobddManager& operator=(BasicRobddManager const&) = delete;

    Diagram constant(Bool value);
    Diagram variable(SizeType index);
    Diagram variable(BddVariable variable);

    Diagram negation(Diagram const& value);
    Diagram conjunction(Diagram const& lhs, Diagram const& rhs);
    Diagram disjunction(Diagram const& lhs, Diagram const& rhs);
    Diagram exclusive_or(Diagram const& lhs, Diagram const& rhs);
    Diagram ite(Diagram const& condition, Diagram const& then_branch, Diagram const& else_branch);

    Bool evaluate(Diagram const& value, BinaryWord const& assignment) const;
    SizeType node_count() const;

  private:
    using NodeRef = typename EdgePolicy::NodeRef;
    using NodeKey = std::tuple<SizeType,NodeRef,NodeRef>;
    using IteKey = std::tuple<NodeRef,NodeRef,NodeRef>;

    struct Node {
        BddVariable variable;
        NodeRef low;
        NodeRef high;
    };

    static constexpr NodeRef FALSE_NODE = EdgePolicy::FALSE_NODE;
    static constexpr NodeRef TRUE_NODE = EdgePolicy::TRUE_NODE;

    NodeRef _validate(Diagram const& value) const;
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

using RegularRobdd = BasicRobdd<RegularEdgePolicy>;
using RegularRobddManager = BasicRobddManager<RegularEdgePolicy>;
using ComplementedRobdd = BasicRobdd<ComplementedEdgePolicy>;
using ComplementedRobddManager = BasicRobddManager<ComplementedEdgePolicy>;

} // namespace Detail
} // namespace Ariadne

#endif // ARIADNE_BDD_DETAIL_ROBDD_IMPL_HPP
