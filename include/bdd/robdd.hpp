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
//! \details The owning RobddManager must outlive every Robdd handle created by it.
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
    using NodeId = SizeType;

    Robdd(RobddManager* manager, NodeId root) : _manager(manager), _root(root) { }

    RobddManager* _manager;
    NodeId _root;

    friend class RobddManager;
};

//! \brief Manager owning the canonical nodes and computed table of ROBDDs.
//! \details The current implementation uses a single mutex as a deliberately
//! simple thread-safe baseline. The data structures will be sharded later.
class RobddManager {
  public:
    RobddManager() = default;
    RobddManager(RobddManager const&) = delete;
    RobddManager& operator=(RobddManager const&) = delete;

    //! \brief Return the Boolean constant.
    Robdd constant(Bool value);

    //! \brief Return the ROBDD for the variable with the given identity.
    Robdd variable(SizeType index);
    Robdd variable(BddVariable variable);

    //! \brief Boolean operations.
    Robdd negation(Robdd const& value);
    Robdd conjunction(Robdd const& lhs, Robdd const& rhs);
    Robdd disjunction(Robdd const& lhs, Robdd const& rhs);
    Robdd exclusive_or(Robdd const& lhs, Robdd const& rhs);
    Robdd ite(Robdd const& condition, Robdd const& then_branch, Robdd const& else_branch);

    //! \brief Evaluate an ROBDD under a total assignment.
    //! \details assignment[i] is the truth value of variable i.
    Bool evaluate(Robdd const& value, BinaryWord const& assignment) const;

    //! \brief Number of unique non-terminal nodes currently owned by the manager.
    SizeType node_count() const;

  private:
    using NodeId = Robdd::NodeId;
    using NodeKey = std::tuple<SizeType,NodeId,NodeId>;
    using IteKey = std::tuple<NodeId,NodeId,NodeId>;

    struct Node {
        BddVariable variable;
        NodeId low;
        NodeId high;
    };

    static constexpr NodeId FALSE_NODE = 0u;
    static constexpr NodeId TRUE_NODE = 1u;

    NodeId _validate(Robdd const& value) const;
    Node _node(NodeId id) const;
    SizeType _top_variable(NodeId id) const;
    std::pair<NodeId,NodeId> _cofactor(NodeId id, SizeType variable) const;

    NodeId _make_node(BddVariable variable, NodeId low, NodeId high);
    NodeId _ite(NodeId condition, NodeId then_branch, NodeId else_branch);

  private:
    mutable std::mutex _mutex;
    std::vector<Node> _nodes;
    std::map<NodeKey,NodeId> _unique_table;
    std::map<IteKey,NodeId> _ite_cache;
};

} // namespace Ariadne

#endif // ARIADNE_BDD_ROBDD_HPP
