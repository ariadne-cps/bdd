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

#include <cstddef>
#include <functional>
#include <map>
#include <mutex>
#include <set>
#include <optional>
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

    //! \brief Return the root variable, or no value for a terminal.
    std::optional<BddVariable> top_variable(Robdd const& value) const;

    //! \brief Return the semantic low/high branch of a non-terminal ROBDD.
    //! \details A complemented root complements both returned branches.
    //! Passing a terminal is a precondition violation.
    Robdd low(Robdd const& value) const;
    Robdd high(Robdd const& value) const;

    //! \brief Build a canonical decision node with the given semantic children.
    //! \details Both children must belong to this manager and all their
    //! variables must follow the supplied variable in the fixed ordering.
    //! Equal children are reduced to their shared child.
    Robdd make_node(BddVariable variable, Robdd const& low, Robdd const& high);

    //! \brief Distinct variables in the function, sorted by increasing index.
    //! \details A complemented root has the same support as its regular root.
    std::vector<BddVariable> support(Robdd const& value) const;

    //! \brief Shift every supported variable by a signed offset.
    //! \details The uniform shift preserves variable order. Negative indices,
    //! overflow and the reserved terminal-order sentinel are rejected.
    Robdd shift(Robdd const& value, std::ptrdiff_t offset);

    //! \brief Simultaneously replace variable indices, keeping unmapped indices.
    //! \details Reordering and variable identification (non-injective maps)
    //! are supported: the result is rebuilt canonically using ITE. Source and
    //! target indices must not equal the reserved terminal-order sentinel.
    Robdd rename(Robdd const& value, std::map<SizeType,SizeType> const& indices);

    //! \brief Restrict a variable to a Boolean value, even below the root.
    //! \details Variables not in the support leave the function unchanged.
    Robdd restrict(Robdd const& value, BddVariable variable, Bool assignment);

    //! \brief Existentially quantify the specified variables.
    //! \details Duplicate/absent variables are allowed. A complemented
    //! reference is interpreted semantically, not by negating its quantified result.
    Robdd exists(Robdd const& value, std::vector<BddVariable> const& variables);

    //! \brief Compute exists(variables, lhs AND rhs) in one recursive traversal.
    //! \details Does not construct the full intermediate conjunction.
    Robdd and_exists(
        Robdd const& lhs, Robdd const& rhs,
        std::vector<BddVariable> const& variables);

    Robdd negation(Robdd const& value);
    Robdd conjunction(Robdd const& lhs, Robdd const& rhs);
    Robdd disjunction(Robdd const& lhs, Robdd const& rhs);
    Robdd exclusive_or(Robdd const& lhs, Robdd const& rhs);
    Robdd ite(Robdd const& condition, Robdd const& then_branch, Robdd const& else_branch);

    Bool evaluate(Robdd const& value, BinaryWord const& assignment) const;

    //! \brief Count satisfying assignments over a finite variable universe.
    //! \details The universe contains variable_count consecutive variables
    //! starting at first_variable_index. Variables absent from the support
    //! are unconstrained (don't-care). Each satisfying assignment is counted
    //! once, including all valuations of unconstrained variables.
    //! Throws if the exact count cannot be represented by SizeType.
    SizeType model_count(
        Robdd const& value, SizeType variable_count,
        SizeType first_variable_index = 0u) const;

    //! \brief Visit all satisfying assignments in lexicographic bit order.
    //! \details Each word has variable_count bits, ordered by increasing
    //! variable index starting at first_variable_index. Unconstrained
    //! variables take both values. The callback receives each word during
    //! traversal; all results are not materialized.
    //! The callback must be nonempty and must not mutate this manager.
    void for_each_satisfying_assignment(
        Robdd const& value, SizeType variable_count,
        std::function<void(BinaryWord const&)> const& visitor,
        SizeType first_variable_index = 0u) const;

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
    SizeType _validate_variable_domain(
        NodeRef root, SizeType variable_count,
        SizeType first_variable_index) const;
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
