/***************************************************************************
 *            bdd.hpp
 *
 *  Copyright  2026  Ariadne contributors
 ****************************************************************************/

/*
 *  This file is part of Ariadne BDD.
 *
 *  Ariadne BDD is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 */

#ifndef ARIADNE_BDD_BDD_HPP
#define ARIADNE_BDD_BDD_HPP

#include <map>
#include <mutex>
#include <tuple>
#include <utility>
#include <vector>

#include "utility/typedefs.hpp"
#include "utility/binary_word.hpp"

namespace Ariadne {

class BddManager;

//! \brief Identifier of a Boolean variable in a BDD manager.
//! \details Variables are ordered by increasing index.
class BddVariable {
  public:
    explicit BddVariable(SizeType index) : _index(index) { }

    SizeType index() const { return _index; }

    friend Bool operator==(BddVariable lhs, BddVariable rhs) { return lhs._index == rhs._index; }
    friend Bool operator!=(BddVariable lhs, BddVariable rhs) { return not (lhs == rhs); }
    friend Bool operator<(BddVariable lhs, BddVariable rhs) { return lhs._index < rhs._index; }

  private:
    SizeType _index;
};

//! \brief Lightweight handle to a reduced ordered binary decision diagram.
//! \details The owning BddManager must outlive every Bdd handle created by it.
class Bdd {
  public:
    Bool is_false() const { return _root == 0u; }
    Bool is_true() const { return _root == 1u; }
    Bool is_terminal() const { return _root <= 1u; }

    friend Bool operator==(Bdd const& lhs, Bdd const& rhs) {
        return lhs._manager == rhs._manager and lhs._root == rhs._root;
    }
    friend Bool operator!=(Bdd const& lhs, Bdd const& rhs) { return not (lhs == rhs); }

  private:
    using NodeId = SizeType;

    Bdd(BddManager* manager, NodeId root) : _manager(manager), _root(root) { }

    BddManager* _manager;
    NodeId _root;

    friend class BddManager;
};

//! \brief Manager owning the canonical nodes and computed table of a ROBDD.
//! \details The current implementation uses a single mutex as a deliberately
//! simple thread-safe baseline. The data structures will be sharded later.
class BddManager {
  public:
    BddManager() = default;
    BddManager(BddManager const&) = delete;
    BddManager& operator=(BddManager const&) = delete;

    //! \brief Return the Boolean constant.
    Bdd constant(Bool value);

    //! \brief Return the BDD for the variable with the given index.
    Bdd variable(SizeType index);
    Bdd variable(BddVariable variable);

    //! \brief Boolean operations.
    Bdd negation(Bdd const& value);
    Bdd conjunction(Bdd const& lhs, Bdd const& rhs);
    Bdd disjunction(Bdd const& lhs, Bdd const& rhs);
    Bdd exclusive_or(Bdd const& lhs, Bdd const& rhs);
    Bdd ite(Bdd const& condition, Bdd const& then_branch, Bdd const& else_branch);

    //! \brief Evaluate a BDD under a total assignment.
    //! \details assignment[i] is the truth value of variable i.
    Bool evaluate(Bdd const& value, BinaryWord const& assignment) const;

    //! \brief Number of unique non-terminal nodes currently owned by the manager.
    SizeType node_count() const;

  private:
    using NodeId = Bdd::NodeId;
    using NodeKey = std::tuple<SizeType,NodeId,NodeId>;
    using IteKey = std::tuple<NodeId,NodeId,NodeId>;

    struct Node {
        BddVariable variable;
        NodeId low;
        NodeId high;
    };

    static constexpr NodeId FALSE_NODE = 0u;
    static constexpr NodeId TRUE_NODE = 1u;

    NodeId _validate(Bdd const& value) const;
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

#endif // ARIADNE_BDD_BDD_HPP
