/***************************************************************************
 *            bdd/robdd.hpp
 *
 *  Copyright  2026  Luca Geretti
 *
 ****************************************************************************/

/*
 *  This file is part of Ariadne BDD.
 *
 *  Ariadne BDD is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Ariadne BDD is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Ariadne BDD. If not, see <https://www.gnu.org/licenses/>.
 */

/** @file bdd/robdd.hpp
 *  @brief Canonical reduced ordered binary decision diagrams and their manager.
 */

#ifndef ARIADNE_BDD_ROBDD_HPP
#define ARIADNE_BDD_ROBDD_HPP

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#include "bdd/variable.hpp"
#include "utility/binary_word.hpp"
#include "utility/typedefs.hpp"

namespace Ariadne {

class RobddManager;

//! \brief Lightweight handle to a reduced ordered binary decision diagram.
//! \details The root is a semantic node reference whose low bit records
//! complement polarity. The owning manager must outlive every handle.
class Robdd {
  public:
    /** @brief Report whether this diagram represents the false terminal. */
    Bool is_false() const { return _root == 0u; }
    /** @brief Report whether this diagram represents the true terminal. */
    Bool is_true() const { return _root == 1u; }
    /** @brief Report whether this diagram is a Boolean terminal. */
    Bool is_terminal() const { return _root <= 1u; }

    /** @brief Compare diagram identity within the same manager. */
    friend Bool operator==(const Robdd& lhs, const Robdd& rhs) {
        return lhs._manager == rhs._manager and lhs._root == rhs._root;
    }
    /** @brief Check whether two diagram handles differ. */
    friend Bool operator!=(const Robdd& lhs, const Robdd& rhs) { return not(lhs == rhs); }

  private:
    using NodeRef = SizeType;

    /** @brief Construct a diagram handle from its owner and semantic root. */
    Robdd(RobddManager* manager, NodeRef root) : _manager(manager), _root(root) {}

    RobddManager* _manager;
    NodeRef _root;

    friend class RobddManager;
};

//! \brief Snapshot of manager allocations, obtained under a single lock.
//! \details Counts and ABI payload sizes are exact for their C++ containers.
//! The total heap footprint is NOT reported: hash-table buckets, map nodes,
//! allocator overhead and any other runtime allocations are excluded.
//! Nodes not reachable from a particular root remain in the manager.
struct RobddManagerStatistics {
    SizeType node_count;
    SizeType node_capacity;
    SizeType node_size_bytes;
    SizeType unique_table_entries;
    SizeType unique_table_entry_payload_bytes;
    SizeType ite_cache_entries;
    SizeType ite_cache_entry_payload_bytes;
};

//! \brief Diagnostic counters for a single instrumented ITE computation.
//! \details Lock wait is summed across threads, and may exceed wall time.
//! A raced cache insertion counts one completed duplicate ITE key,
//! not the full amount of computation duplicated in its subtree.
struct RobddIteProfile {
    SizeType calls = 0u;
    SizeType cache_lookups = 0u;
    SizeType cache_hits = 0u;
    SizeType cache_raced_inserts = 0u;
    SizeType cache_lock_calls = 0u;
    SizeType cache_lock_contended = 0u;
    // Operand values copied by the batched cache+node snapshot.
    SizeType node_snapshot_reads = 0u;
    // Legacy separate-node-lock metrics remain zero for batched ITE.
    SizeType node_lock_calls = 0u;
    SizeType node_lock_contended = 0u;
    SizeType unique_lock_calls = 0u;
    SizeType unique_lock_contended = 0u;
    SizeType tasks_forked = 0u;
    std::uint64_t cache_lock_wait_ns = 0u;
    std::uint64_t node_lock_wait_ns = 0u;
    std::uint64_t unique_lock_wait_ns = 0u;
    std::uint64_t task_submit_ns = 0u;
    std::uint64_t task_join_ns = 0u;
    std::uint64_t task_start_delay_ns = 0u;

    /** @brief Add the counters of a child ITE computation to this profile. */
    void accumulate(const RobddIteProfile& child) {
        calls += child.calls;
        cache_lookups += child.cache_lookups;
        cache_hits += child.cache_hits;
        cache_raced_inserts += child.cache_raced_inserts;
        cache_lock_calls += child.cache_lock_calls;
        cache_lock_contended += child.cache_lock_contended;
        node_snapshot_reads += child.node_snapshot_reads;
        node_lock_calls += child.node_lock_calls;
        node_lock_contended += child.node_lock_contended;
        unique_lock_calls += child.unique_lock_calls;
        unique_lock_contended += child.unique_lock_contended;
        tasks_forked += child.tasks_forked;
        cache_lock_wait_ns += child.cache_lock_wait_ns;
        node_lock_wait_ns += child.node_lock_wait_ns;
        unique_lock_wait_ns += child.unique_lock_wait_ns;
        task_submit_ns += child.task_submit_ns;
        task_join_ns += child.task_join_ns;
        task_start_delay_ns += child.task_start_delay_ns;
    }
};

//! \brief Manager owning canonical ROBDD nodes and the ITE computed table.
//! \details Complemented edges are the single internal representation.
//! Physical nodes are canonicalized with a regular low reference.
class RobddManager {
  public:
    //! \brief Construct an empty manager for canonical ROBDDs.
    //! \details The manager owns all nodes and must outlive its Robdd handles.
    RobddManager() = default;
    /** @brief Disallow copying a manager that owns canonical nodes. */
    RobddManager(const RobddManager&) = delete;
    /** @brief Disallow copy assignment of canonical node ownership. */
    RobddManager& operator=(const RobddManager&) = delete;

    //! \brief Return the Boolean constant false or true.
    //! \param value The truth value represented by the returned terminal.
    //! \return A terminal ROBDD owned by this manager.
    Robdd constant(Bool value);

    //! \brief Return the canonical decision diagram for a variable index.
    //! \param index The variable index in the fixed increasing order.
    //! \return The Boolean function that equals the selected variable.
    //! \pre index must be smaller than the reserved maximum SizeType index.
    Robdd variable(SizeType index);

    //! \brief Return the canonical decision diagram for a variable.
    //! \param variable The variable to represent.
    //! \return The Boolean function that equals the selected variable.
    //! \pre The variable index must not be the terminal-order sentinel.
    Robdd variable(BddVariable variable);

    //! \brief Return the root variable, or no value for a terminal.
    std::optional<BddVariable> top_variable(const Robdd& value) const;

    //! \brief Return the low cofactor of a non-terminal ROBDD.
    //! \details Fixes the root variable to false, propagating the
    //! complement polarity of the incoming reference to the result.
    //! \pre value must belong to this manager and be non-terminal.
    Robdd low(const Robdd& value) const;

    //! \brief Return the high cofactor of a non-terminal ROBDD.
    //! \details Fixes the root variable to true, propagating the
    //! complement polarity of the incoming reference to the result.
    //! \pre value must belong to this manager and be non-terminal.
    Robdd high(const Robdd& value) const;

    //! \brief Build a canonical decision node with the given semantic children.
    //! \details Both children must belong to this manager and all their
    //! variables must follow the supplied variable in the fixed ordering.
    //! Equal children are reduced to their shared child.
    Robdd make_node(BddVariable variable, const Robdd& low, const Robdd& high);

    //! \brief Distinct variables in the function, sorted by increasing index.
    //! \details A complemented root has the same support as its regular root.
    std::vector<BddVariable> support(const Robdd& value) const;

    //! \brief Shift every supported variable by a signed offset.
    //! \details The uniform shift preserves variable order. Negative indices,
    //! overflow and the reserved terminal-order sentinel are rejected.
    Robdd shift(const Robdd& value, std::ptrdiff_t offset);

    //! \brief Simultaneously replace variable indices, keeping unmapped indices.
    //! \details Reordering and variable identification (non-injective maps)
    //! are supported: the result is rebuilt canonically using ITE. Source and
    //! target indices must not equal the reserved terminal-order sentinel.
    Robdd rename(const Robdd& value, const std::map<SizeType, SizeType>& indices);

    //! \brief Restrict a variable to a Boolean value, even below the root.
    //! \details Variables not in the support leave the function unchanged.
    Robdd restrict(const Robdd& value, BddVariable variable, Bool assignment);

    //! \brief Existentially quantify the specified variables.
    //! \details Duplicate/absent variables are allowed. A complemented
    //! reference is interpreted semantically, not by negating its quantified result.
    Robdd exists(const Robdd& value, const std::vector<BddVariable>& variables);

    //! \brief Compute exists(variables, lhs AND rhs) in one recursive traversal.
    //! \details Does not construct the full intermediate conjunction.
    Robdd and_exists(
        const Robdd& lhs, const Robdd& rhs,
        const std::vector<BddVariable>& variables);

    //! \brief Compute the Boolean complement of a function.
    //! \details Complemented edges make negation a constant-time polarity
    //! change without allocating a physical decision node.
    //! \param value The function to complement.
    //! \return The canonical ROBDD representing NOT value.
    //! \pre value must belong to this manager.
    Robdd negation(const Robdd& value);

    //! \brief Compute the Boolean conjunction of two functions.
    //! \return The canonical ROBDD representing lhs AND rhs.
    //! \pre Both operands must belong to this manager.
    Robdd conjunction(const Robdd& lhs, const Robdd& rhs);

    //! \brief Compute the Boolean disjunction of two functions.
    //! \return The canonical ROBDD representing lhs OR rhs.
    //! \pre Both operands must belong to this manager.
    Robdd disjunction(const Robdd& lhs, const Robdd& rhs);

    //! \brief Compute the exclusive disjunction of two functions.
    //! \return The canonical ROBDD representing lhs XOR rhs.
    //! \pre Both operands must belong to this manager.
    Robdd exclusive_or(const Robdd& lhs, const Robdd& rhs);

    //! \brief Compute the if-then-else operation on Boolean functions.
    //! \details The result represents
    //! (condition AND then_branch) OR (NOT condition AND else_branch).
    //! The operation is reduced and canonical for the manager's fixed
    //! variable order; its recursive results are memoized internally.
    //! \param condition The function choosing between the two branches.
    //! \param then_branch The function selected where condition is true.
    //! \param else_branch The function selected where condition is false.
    //! \return The canonical ROBDD for the conditional Boolean function.
    //! \pre All three operands must belong to this manager.
    Robdd ite(const Robdd& condition, const Robdd& then_branch, const Robdd& else_branch);

    //! \brief Compute if-then-else by reducing independent cofactors in parallel.
    //! \details Opt-in bounded fork/join over the same canonical manager as ite().
    //! Uses the shared ThreadManager, falling back to serial ITE when its
    //! concurrency is less than two. The manager's unique table and computed
    //! table remain synchronised; no second manager or worker pool is created.
    //! The caller must not destroy or reconfigure the manager or ThreadManager
    //! while the operation is in progress.
    //! \pre All three operands must belong to this manager.
    Robdd ite_parallel(
        const Robdd& condition, const Robdd& then_branch, const Robdd& else_branch);

    //! \brief Diagnose the serial or bounded-parallel ITE on this manager.
    //! \details An explicitly instrumented recursion; never selected by ite()
    //! or ite_parallel(). The result remains canonical. Locks, cache races,
    //! and task lifecycle costs are counted; instrumentation itself affects
    //! performance and its times must NOT be treated as production timings.
    //! The caller must not mutate the output counters until the call returns.
    Robdd ite_profiled(
        const Robdd& condition, const Robdd& then_branch, const Robdd& else_branch,
        Bool parallel, RobddIteProfile& counters);

    //! \brief Evaluate a function under a Boolean assignment.
    //! \details assignment[i] gives the truth value of variable i;
    //! indices absent from the function's support are ignored.
    //! Complemented edges are interpreted according to their polarity.
    //! \param value The function to evaluate.
    //! \param assignment An assignment providing all supported indices.
    //! \return The truth value of the function under the assignment.
    //! \pre value must belong to this manager and assignment must contain
    //! an entry for every variable in the function's support.
    Bool evaluate(const Robdd& value, const BinaryWord& assignment) const;

    //! \brief Count satisfying assignments over a finite variable universe.
    //! \details The universe contains variable_count consecutive variables
    //! starting at first_variable_index. Variables absent from the support
    //! are unconstrained (don't-care). Each satisfying assignment is counted
    //! once, including all valuations of unconstrained variables.
    //! Throws if the exact count cannot be represented by SizeType.
    SizeType model_count(
        const Robdd& value, SizeType variable_count,
        SizeType first_variable_index = 0u) const;

    //! \brief Visit all satisfying assignments in lexicographic bit order.
    //! \details Each word has variable_count bits, ordered by increasing
    //! variable index starting at first_variable_index. Unconstrained
    //! variables take both values. The callback receives each word during
    //! traversal; all results are not materialized.
    //! The callback must be nonempty and must not mutate this manager.
    void for_each_satisfying_assignment(
        const Robdd& value, SizeType variable_count,
        const std::function<void(const BinaryWord&)>& visitor,
        SizeType first_variable_index = 0u) const;

    //! \brief Return the total number of physical non-terminal nodes allocated.
    //! \details The count covers the manager's entire canonical node table,
    //! including nodes that are not reachable from any particular root.
    //! It is not the size of a single ROBDD or a count of satisfying models.
    SizeType node_count() const;

    //! \brief Snapshot node vector capacity, uniqueness table and ITE cache.
    //! \details Safe to call concurrently with canonical node insertion.
    //! Returned sizes exclude map bookkeeping and allocator overhead.
    RobddManagerStatistics statistics() const;

  private:
    using NodeRef = Robdd::NodeRef;
    using NodeKey = std::tuple<SizeType, NodeRef, NodeRef>;
    using IteKey = std::tuple<NodeRef, NodeRef, NodeRef>;

    // A hash collision must never imply canonical equality: unordered_map
    // still compares all three NodeKey members through tuple equality.
    // All lookups, insertions and rehashes remain under the original _mutex.
    /** @brief Hash a canonical node key without compromising full-key equality. */
    struct NodeKeyHash {
        /** @brief Compute a hash from a node's variable and its two child references. */
        std::size_t operator()(const NodeKey& key) const noexcept {
            constexpr std::size_t salt =
                static_cast<std::size_t>(0x9e3779b97f4a7c15ULL);
            std::size_t seed = 0u;
            auto combine = [&seed](SizeType value) {
                seed ^= std::hash<SizeType>{}(value) + salt + (seed << 6u) + (seed >> 2u);
            };
            combine(std::get<0>(key));
            combine(std::get<1>(key));
            combine(std::get<2>(key));
            return seed;
        }
    };

    /** @brief Store one canonical decision node and its two children. */
    struct Node {
        BddVariable variable;
        NodeRef low;
        NodeRef high;
    };
    // Snapshot of an ITE cache lookup and cofactor decomposition;
    // only value copies are retained outside the manager mutex.
    /** @brief Record a cache lookup and the three cofactor splits for one ITE step. */
    struct IteStep {
        Bool cached = false;
        NodeRef cached_result = 0u;
        SizeType variable = 0u;
        std::pair<NodeRef, NodeRef> condition;
        std::pair<NodeRef, NodeRef> then_branch;
        std::pair<NodeRef, NodeRef> else_branch;
    };

    static constexpr NodeRef FALSE_NODE = 0u;
    static constexpr NodeRef TRUE_NODE = 1u;

    /** @brief Check whether an internal reference is a terminal. */
    static Bool _is_terminal(NodeRef ref) { return ref <= TRUE_NODE; }
    /** @brief Check the complement bit of a node reference. */
    static Bool _is_complemented(NodeRef ref) { return (ref & 1u) != 0u; }
    /** @brief Flip the complement bit without allocating a new node. */
    static NodeRef _negate(NodeRef ref) { return ref ^ 1u; }
    /** @brief Decode the physical node index from its reference. */
    static SizeType _node_index(NodeRef ref) { return (ref >> 1u) - 1u; }
    /** @brief Encode an allocated node index as a regular reference. */
    static NodeRef _make_regular_node_ref(SizeType index) { return (index + 1u) << 1u; }

    /** @brief Validate manager ownership and return the semantic root reference. */
    NodeRef _validate(const Robdd& value) const;
    /** @brief Validate that the requested variable universe covers the function support. */
    SizeType _validate_variable_domain(
        NodeRef root, SizeType variable_count,
        SizeType first_variable_index) const;
    /** @brief Read the immutable canonical node denoted by a reference. */
    Node _node(NodeRef ref) const;
    /** @brief Return the top variable index or the terminal-order sentinel. */
    SizeType _top_variable(NodeRef ref) const;
    /** @brief Return low and high cofactors at a chosen variable. */
    std::pair<NodeRef, NodeRef> _cofactor(NodeRef ref, SizeType variable) const;

    /** @brief Canonicalize a decision node, reducing equal branches. */
    NodeRef _make_node(BddVariable variable, NodeRef low, NodeRef high);
    // _unlocked requires _mutex, and the wrapper acquires it.
    /** @brief Inspect the computed table and cofactor nodes while holding the manager lock. */
    IteStep _ite_lookup_split_unlocked(
        NodeRef condition, NodeRef then_branch, NodeRef else_branch) const;
    /** @brief Acquire the manager lock and snapshot an ITE computation step. */
    IteStep _ite_lookup_split(
        NodeRef condition, NodeRef then_branch, NodeRef else_branch) const;
    /** @brief Recursively compute canonical if-then-else using the computed table. */
    NodeRef _ite(NodeRef condition, NodeRef then_branch, NodeRef else_branch);
    /** @brief Compute if-then-else using bounded parallel task decomposition. */
    NodeRef _ite_parallel(
        NodeRef condition, NodeRef then_branch, NodeRef else_branch,
        unsigned forks_remaining);
    /** @brief Canonicalize a node while collecting synchronization statistics. */
    NodeRef _make_node_profiled(
        BddVariable variable, NodeRef low, NodeRef high, RobddIteProfile& counters);
    /** @brief Compute if-then-else while collecting diagnostic counters. */
    NodeRef _ite_profiled(
        NodeRef condition, NodeRef then_branch, NodeRef else_branch,
        RobddIteProfile& counters, unsigned forks_remaining);

  private:
    mutable std::mutex _mutex;
    std::vector<Node> _nodes;
#if defined(ARIADNE_BDD_UNIQUE_TABLE_ORDERED)
    std::map<NodeKey, NodeRef> _unique_table;
#else
    std::unordered_map<NodeKey, NodeRef, NodeKeyHash> _unique_table;
#endif
    std::map<IteKey, NodeRef> _ite_cache;
};

} // namespace Ariadne

#endif // ARIADNE_BDD_ROBDD_HPP
