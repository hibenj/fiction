/*
 * Copyright (c) 2018 - 2023 Marcel Walter
 * Copyright (c) 2023 - present Chair for Design Automation, Technical University of Munich
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

/**
 * @file
 * @brief Enumerates planar cells, small crossing-free gate structures, for every small Boolean function and stores
 * them in a database keyed by negation classes.
 * @author Benjamin Hien (hibenj)
 */

#pragma once

#include "fiction/utils/progress.hpp"

#include <kitty/bit_operations.hpp>
#include <kitty/dynamic_truth_table.hpp>
#include <kitty/operations.hpp>
#include <mockturtle/utils/stopwatch.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

namespace fiction::synthesis
{

/**
 * Gate types a planar cell may consist of. Every gate may read inverted signals: an inversion is an attribute of the
 * edge and counts one unit of cost, like a gate.
 */
enum class planar_gate : uint8_t
{
    /**
     * Two-input conjunction.
     */
    AND = 0,
    /**
     * Two-input disjunction.
     */
    OR = 1,
    /**
     * Two-input exclusive disjunction.
     */
    XOR = 2,
    /**
     * Three-input majority.
     */
    MAJ = 3
};

/**
 * Parameters of the planar cell enumeration.
 */
struct planar_cell_database_params
{
    /**
     * Largest number of inputs a cell may have. The database then holds one table per support size from two up to
     * this number, so that a function of fewer inputs is served by the table of its support.
     */
    uint32_t num_inputs = 4;
    /**
     * Largest number of gate levels of a cell, the inputs not counted. Structures are grown one level at a time; the
     * last level joins whatever signals are still unconsumed.
     */
    uint32_t max_levels = 3;
    /**
     * Gates a cell may consist of. Three-input nodes exist only when `planar_gate::MAJ` is in the set.
     */
    std::vector<planar_gate> gate_set{planar_gate::AND, planar_gate::OR, planar_gate::XOR, planar_gate::MAJ};
    /** @brief Reports the structures evaluated per support size. */
    utils::progress_callback on_progress{};
};

/**
 * Statistics of the planar cell enumeration, one row per support size.
 */
struct planar_cell_database_stats
{
    /**
     * The numbers of one support size.
     */
    struct table_stats
    {
        /**
         * Support size.
         */
        uint32_t num_inputs{0};
        /**
         * Planar structures grown for this support size, before gates and inversions are assigned.
         */
        uint64_t structures{0};
        /**
         * Gate and inversion assignments evaluated.
         */
        uint64_t evaluations{0};
        /**
         * Negation classes of functions that depend on all inputs.
         */
        uint64_t classes{0};
        /**
         * Negation classes with a planar cell.
         */
        uint64_t covered{0};
    };
    /**
     * Per support size, smallest first.
     */
    std::vector<table_stats> tables{};
    /**
     * Total runtime.
     */
    mockturtle::stopwatch<>::duration time_total{0};
    /**
     * Prints the statistics.
     *
     * @param out Output stream.
     */
    void report(std::ostream& out = std::cout) const
    {
        for (const auto& t : tables)
        {
            out << "[i] " << t.num_inputs << " inputs: " << t.structures << " structures, " << t.evaluations
                << " evaluations, " << t.covered << " of " << t.classes << " negation classes covered\n";
        }
        out << "[i] total time = " << mockturtle::to_seconds(time_total) << " s\n";
    }
};

/**
 * One node of a planar cell.
 */
struct planar_cell_node
{
    /**
     * Signals this node reads, in the left-to-right order of the cell. Signal `i < num_inputs` is input `i`, signal
     * `num_inputs + j` is node `j`.
     */
    std::vector<uint8_t> fanins{};
    /**
     * Level of the node; inputs sit on level 0.
     */
    uint8_t level{1};
    /**
     * Gate the node computes.
     */
    planar_gate gate{planar_gate::AND};
    /**
     * Bit `i` set means fanin `i` is read inverted.
     */
    uint8_t inverted_fanins{0};
};

/**
 * A planar cell: a small gate structure whose nodes can be drawn level by level without any crossing edge. The
 * nodes are stored in creation order, which is a topological order; the last node is the output. Every node reads
 * only signals that neighbour each other on the frontier of unconsumed signals at the time the node is created, so
 * the drawing with the inputs in order `0..num_inputs - 1` from left to right is planar, and an edge from a signal
 * more than one level below is a wire that passes the levels in between.
 */
struct planar_cell
{
    /**
     * Number of inputs.
     */
    uint8_t num_inputs{0};
    /**
     * Nodes in topological order, the last one is the output.
     */
    std::vector<planar_cell_node> nodes{};
    /**
     * Whether the output of the last node is inverted.
     */
    bool output_inverted{false};
    /**
     * Number of gates.
     *
     * @return Number of nodes.
     */
    [[nodiscard]] uint32_t num_gates() const noexcept
    {
        return static_cast<uint32_t>(nodes.size());
    }
    /**
     * Number of inverted edges, the output inversion included.
     *
     * @return Number of inversions.
     */
    [[nodiscard]] uint32_t num_inversions() const noexcept
    {
        uint32_t inversions = output_inverted ? 1u : 0u;
        for (const auto& n : nodes)
        {
            inversions += static_cast<uint32_t>(std::popcount(n.inverted_fanins));
        }
        return inversions;
    }
    /**
     * Cost of the cell: gates plus inversions, both being nodes in field-coupled nanotechnologies.
     *
     * @return Cost.
     */
    [[nodiscard]] uint32_t cost() const noexcept
    {
        return num_gates() + num_inversions();
    }
    /**
     * Number of gate levels.
     *
     * @return Level of the output node, or 0 for a cell without nodes.
     */
    [[nodiscard]] uint32_t depth() const noexcept
    {
        return nodes.empty() ? 0u : nodes.back().level;
    }
    /**
     * Number of edges.
     *
     * @return Sum of the fanin counts.
     */
    [[nodiscard]] uint32_t num_edges() const noexcept
    {
        uint32_t edges = 0;
        for (const auto& n : nodes)
        {
            edges += static_cast<uint32_t>(n.fanins.size());
        }
        return edges;
    }
    /**
     * Whether `a` is cheaper than `b`: fewer gates plus inversions, then fewer levels, then fewer edges.
     *
     * @param a First cell.
     * @param b Second cell.
     * @return `true` iff `a` is preferred over `b`.
     */
    [[nodiscard]] static bool cheaper(const planar_cell& a, const planar_cell& b) noexcept
    {
        return std::make_tuple(a.cost(), a.depth(), a.num_edges()) <
               std::make_tuple(b.cost(), b.depth(), b.num_edges());
    }
    /**
     * The function the cell computes, as a truth table word over its inputs (input `i` is variable `i`). Cells with
     * up to six inputs are supported.
     *
     * @return Truth table of the output, bit `m` for the input minterm `m`.
     */
    [[nodiscard]] uint64_t function() const;
    /**
     * The function the cell computes as a truth table.
     *
     * @return Truth table over `num_inputs` variables.
     */
    [[nodiscard]] kitty::dynamic_truth_table truth_table() const
    {
        kitty::dynamic_truth_table tt{num_inputs};
        *tt.begin() = function();
        return tt;
    }
    /**
     * A one-line description of the cell: one `gate(fanins)@level` term per node, an inverted fanin carries a `~`,
     * an inverted output a trailing `~`.
     *
     * @return Description.
     */
    [[nodiscard]] std::string to_string() const
    {
        static constexpr std::array<const char*, 4> names{"and", "or", "xor", "maj"};

        std::string s{};
        for (const auto& n : nodes)
        {
            s += names.at(static_cast<std::size_t>(n.gate));
            s += '(';
            for (std::size_t i = 0; i < n.fanins.size(); ++i)
            {
                if (i > 0)
                {
                    s += ',';
                }
                if (((n.inverted_fanins >> i) & 1u) != 0u)
                {
                    s += '~';
                }
                s += std::to_string(n.fanins[i]);
            }
            s += ")@";
            s += std::to_string(n.level);
            s += ' ';
        }
        if (output_inverted)
        {
            s += '~';
        }
        return s;
    }
};

namespace detail
{

/**
 * Truth table words over up to six variables.
 */
struct tt_word
{
    /**
     * Bits where variable `i` is 0.
     */
    static constexpr std::array<uint64_t, 6> VAR_ZERO{0x5555555555555555ull, 0x3333333333333333ull,
                                                      0x0F0F0F0F0F0F0F0Full, 0x00FF00FF00FF00FFull,
                                                      0x0000FFFF0000FFFFull, 0x00000000FFFFFFFFull};
    /**
     * Mask of the valid bits of a word over `k` variables.
     */
    [[nodiscard]] static constexpr uint64_t full(const uint32_t k) noexcept
    {
        return k >= 6u ? ~0ull : ((1ull << (1u << k)) - 1ull);
    }
    /**
     * The projection of variable `i`.
     */
    [[nodiscard]] static constexpr uint64_t var(const uint32_t i, const uint32_t k) noexcept
    {
        return ~VAR_ZERO.at(i) & full(k);
    }
    /**
     * `t` with variable `i` negated.
     */
    [[nodiscard]] static constexpr uint64_t flip(const uint64_t t, const uint32_t i) noexcept
    {
        const auto shift = 1u << i;
        return ((t & VAR_ZERO.at(i)) << shift) | ((t >> shift) & VAR_ZERO.at(i));
    }
    /**
     * `t` with every variable in `mask` negated.
     */
    [[nodiscard]] static constexpr uint64_t flip_all(uint64_t t, const uint32_t mask, const uint32_t k) noexcept
    {
        for (uint32_t i = 0; i < k; ++i)
        {
            if (((mask >> i) & 1u) != 0u)
            {
                t = flip(t, i);
            }
        }
        return t;
    }
    /**
     * Whether `t` depends on every one of the `k` variables.
     */
    [[nodiscard]] static constexpr bool has_full_support(const uint64_t t, const uint32_t k) noexcept
    {
        for (uint32_t i = 0; i < k; ++i)
        {
            if (flip(t, i) == t)
            {
                return false;
            }
        }
        return true;
    }
};

/**
 * The negation class of a function: the smallest word reachable by negating inputs and the output. Negating the
 * inputs in `input_mask` of `f` and then, if `output_negated`, complementing gives `representative`.
 */
struct negation_class
{
    uint64_t representative{0};
    uint32_t input_mask{0};
    bool     output_negated{false};
};

/**
 * Canonizes `t` under input and output negation.
 */
[[nodiscard]] inline negation_class canonize(const uint64_t t, const uint32_t k) noexcept
{
    const auto full = tt_word::full(k);

    negation_class best{.representative = ~0ull, .input_mask = 0, .output_negated = false};
    for (uint32_t mask = 0; mask < (1u << k); ++mask)
    {
        const auto flipped = tt_word::flip_all(t, mask, k) & full;
        if (flipped < best.representative)
        {
            best = negation_class{.representative = flipped, .input_mask = mask, .output_negated = false};
        }
        if (const auto negated = ~flipped & full; negated < best.representative)
        {
            best = negation_class{.representative = negated, .input_mask = mask, .output_negated = true};
        }
    }
    return best;
}

/**
 * Toggles the inversion of every edge that leaves an input in `mask`, and the output inversion if `output`, so that
 * a cell computing `f` computes `f` with those inputs negated.
 */
inline void apply_negations(planar_cell& cell, const uint32_t mask, const bool output) noexcept
{
    for (auto& n : cell.nodes)
    {
        for (std::size_t i = 0; i < n.fanins.size(); ++i)
        {
            if (n.fanins[i] < cell.num_inputs && ((mask >> n.fanins[i]) & 1u) != 0u)
            {
                n.inverted_fanins ^= static_cast<uint8_t>(1u << i);
            }
        }
    }
    cell.output_inverted = cell.output_inverted != output;
}

/**
 * The number of inversions a cell has after `apply_negations(cell, mask, output)`, without modifying it.
 */
[[nodiscard]] inline uint32_t inversions_after(const planar_cell& cell, const uint32_t mask, const bool output) noexcept
{
    uint32_t inversions = (cell.output_inverted != output) ? 1u : 0u;
    for (const auto& n : cell.nodes)
    {
        for (std::size_t i = 0; i < n.fanins.size(); ++i)
        {
            const bool inverted = ((n.inverted_fanins >> i) & 1u) != 0u;
            const bool toggled  = n.fanins[i] < cell.num_inputs && ((mask >> n.fanins[i]) & 1u) != 0u;
            if (inverted != toggled)
            {
                ++inversions;
            }
        }
    }
    return inversions;
}

/**
 * The cost of serving a whole negation class with one cell that computes the class representative: the sum, over
 * the distinct functions of the class, of the gates plus the inversions of the cell instantiated for that function
 * with the cheapest choice of input and output negations. A cell with an input read twice pays twice for negating
 * it, so this prefers cells whose members need few inverted edges on average, not cells that happen to be cheap for
 * the representative alone.
 */
[[nodiscard]] inline uint64_t class_cost(const planar_cell& cell) noexcept
{
    const auto k         = static_cast<uint32_t>(cell.num_inputs);
    const auto full      = tt_word::full(k);
    const auto rep       = cell.function();
    const auto num_gates = cell.num_gates();

    // cheapest inversion count per distinct member function
    std::vector<std::pair<uint64_t, uint32_t>> members{};
    members.reserve(1ull << (k + 1));
    for (uint32_t mask = 0; mask < (1u << k); ++mask)
    {
        const auto flipped = tt_word::flip_all(rep, mask, k) & full;
        for (const bool output : {false, true})
        {
            const auto member     = output ? (~flipped & full) : flipped;
            const auto inversions = inversions_after(cell, mask, output);
            bool       seen       = false;
            for (auto& [m, inv] : members)
            {
                if (m == member)
                {
                    inv  = std::min(inv, inversions);
                    seen = true;
                    break;
                }
            }
            if (!seen)
            {
                members.emplace_back(member, inversions);
            }
        }
    }

    uint64_t cost = 0;
    for (const auto& [m, inv] : members)
    {
        cost += num_gates + inv;
    }
    return cost;
}

}  // namespace detail

inline uint64_t planar_cell::function() const
{
    assert(num_inputs <= 6);

    const auto k    = static_cast<uint32_t>(num_inputs);
    const auto full = detail::tt_word::full(k);

    std::vector<uint64_t> values{};
    values.reserve(k + nodes.size());
    for (uint32_t i = 0; i < k; ++i)
    {
        values.push_back(detail::tt_word::var(i, k));
    }
    for (const auto& n : nodes)
    {
        std::array<uint64_t, 3> in{0, 0, 0};
        for (std::size_t i = 0; i < n.fanins.size(); ++i)
        {
            const auto v = values.at(n.fanins[i]);
            in.at(i)     = ((n.inverted_fanins >> i) & 1u) != 0u ? (~v & full) : v;
        }
        uint64_t out = 0;
        switch (n.gate)
        {
            case planar_gate::AND:
            {
                out = in[0] & in[1];
                break;
            }
            case planar_gate::OR:
            {
                out = in[0] | in[1];
                break;
            }
            case planar_gate::XOR:
            {
                out = in[0] ^ in[1];
                break;
            }
            case planar_gate::MAJ:
            {
                out = (in[0] & in[1]) | (in[0] & in[2]) | (in[1] & in[2]);
                break;
            }
        }
        values.push_back(out & full);
    }

    const auto f = values.empty() ? 0ull : values.back();
    return output_inverted ? (~f & full) : f;
}

/**
 * A database of planar cells: for every negation class (functions that differ only by negating inputs or the
 * output) of every support size up to `num_inputs`, the cheapest planar cell found, computing the class
 * representative. A concrete function is served by the cell of its class with the negations realized as inverted
 * edges, see `lookup`.
 */
class planar_cell_database
{
  public:
    /**
     * A cell instantiated for a concrete function: `cell` computes the function of `support`, where cell input `i`
     * is the function's variable `support[i]`, with the negations already folded into the cell's edges.
     */
    struct instance
    {
        /**
         * The cell, negations applied.
         */
        planar_cell cell{};
        /**
         * The function's variables the cell inputs correspond to, in cell input order.
         */
        std::vector<uint8_t> support{};
    };
    /**
     * A stored cell with the cost of serving its class, see `detail::class_cost`.
     */
    struct entry
    {
        /**
         * The cell, computing the class representative.
         */
        planar_cell cell{};
        /**
         * Sum of the instantiated costs over the members of the class.
         */
        uint64_t class_cost{0};
    };
    /**
     * Creates an empty database with tables for support sizes `2` to `num_inputs`.
     *
     * @param num_inputs Largest support size; at most 6.
     */
    explicit planar_cell_database(const uint32_t num_inputs = 4) : k{num_inputs}, tables(num_inputs + 1)
    {
        assert(num_inputs >= 2 && num_inputs <= 6);
    }
    /**
     * Largest support size.
     *
     * @return Number of inputs.
     */
    [[nodiscard]] uint32_t num_inputs() const noexcept
    {
        return k;
    }
    /**
     * Offers a cell. Its function is canonized; if the function does not depend on all inputs of the cell, the cell
     * is dropped (the smaller support has its own table). Otherwise the cell, normalized to compute the class
     * representative, is stored if its class had no cell yet or the cell serves the class cheaper than the stored
     * one (`detail::class_cost`, ties broken by depth, then by edges).
     *
     * @param cell Cell to offer. Its `num_inputs` selects the table.
     * @return `true` iff the cell was stored.
     */
    bool insert(planar_cell cell)
    {
        const auto n = static_cast<uint32_t>(cell.num_inputs);
        assert(n >= 2 && n <= k);

        const auto f = cell.function();
        if (!detail::tt_word::has_full_support(f, n))
        {
            return false;
        }

        const auto cls = detail::canonize(f, n);
        detail::apply_negations(cell, cls.input_mask, cls.output_negated);
        assert(cell.function() == cls.representative);

        auto& table = tables[n];
        if (const auto it = table.find(cls.representative); it != table.end())
        {
            const auto cost = detail::class_cost(cell);
            if (std::make_tuple(cost, cell.depth(), cell.num_edges()) >=
                std::make_tuple(it->second.class_cost, it->second.cell.depth(), it->second.cell.num_edges()))
            {
                return false;
            }
            it->second = {std::move(cell), cost};
            return true;
        }
        const auto cost = detail::class_cost(cell);
        table.emplace(cls.representative, entry{std::move(cell), cost});
        return true;
    }
    /**
     * Number of stored cells, that is covered negation classes, of one support size.
     *
     * @param num_inputs Support size.
     * @return Number of cells.
     */
    [[nodiscard]] std::size_t num_cells(const uint32_t num_inputs) const noexcept
    {
        return num_inputs < tables.size() ? tables[num_inputs].size() : 0u;
    }
    /**
     * Number of stored cells over all support sizes.
     *
     * @return Number of cells.
     */
    [[nodiscard]] std::size_t num_cells() const noexcept
    {
        std::size_t n = 0;
        for (const auto& t : tables)
        {
            n += t.size();
        }
        return n;
    }
    /**
     * The stored cell of a class representative.
     *
     * @param representative Representative word (see `detail::canonize`) over `num_inputs` variables.
     * @param num_inputs Support size.
     * @return The cell, or `nullptr` if the class has none.
     */
    [[nodiscard]] const planar_cell* cell_of(const uint64_t representative, const uint32_t num_inputs) const noexcept
    {
        if (num_inputs >= tables.size())
        {
            return nullptr;
        }
        const auto it = tables[num_inputs].find(representative);
        return it == tables[num_inputs].end() ? nullptr : &it->second.cell;
    }
    /**
     * Applies `fn` to every stored cell of one support size, with its class representative first.
     *
     * @tparam Fn Functor type.
     * @param num_inputs Support size.
     * @param fn Functor `(uint64_t representative, const planar_cell& cell)`.
     */
    template <typename Fn>
    void foreach_cell(const uint32_t num_inputs, Fn fn) const
    {
        if (num_inputs >= tables.size())
        {
            return;
        }
        for (const auto& [rep, e] : tables[num_inputs])
        {
            fn(rep, e.cell);
        }
    }
    /**
     * Finds the cell of a concrete function. The function is reduced to its support; a constant or a single
     * variable gets a cell without nodes (its output inversion set for the complement); otherwise the cell of the
     * support's negation class is returned with the function's negations folded into its edges, so that
     * `instance.cell.function()` equals the function restricted to `instance.support`.
     *
     * @param f Function over at most `num_inputs()` effective variables (the table may be wider).
     * @return The instance, or `std::nullopt` if the class has no cell.
     */
    [[nodiscard]] std::optional<instance> lookup(const kitty::dynamic_truth_table& f) const
    {
        assert(f.num_vars() <= 6);

        auto       reduced = f;
        const auto support = kitty::min_base_inplace(reduced);
        const auto n       = static_cast<uint32_t>(support.size());

        instance inst{};
        inst.support.assign(support.begin(), support.end());

        if (n <= 1)
        {
            inst.cell.num_inputs      = static_cast<uint8_t>(n);
            inst.cell.output_inverted = n == 0 ? !kitty::is_const0(reduced) : kitty::get_bit(reduced, 0) != 0;
            return inst;
        }
        if (n > k)
        {
            return std::nullopt;
        }

        const auto  word = *reduced.begin() & detail::tt_word::full(n);
        const auto  cls  = detail::canonize(word, n);
        const auto* c    = cell_of(cls.representative, n);
        if (c == nullptr)
        {
            return std::nullopt;
        }

        // the cheapest negations that turn the representative into the function
        uint32_t best_mask   = cls.input_mask;
        bool     best_output = cls.output_negated;
        uint32_t best_cost   = detail::inversions_after(*c, best_mask, best_output);
        for (uint32_t mask = 0; mask < (1u << n); ++mask)
        {
            const auto flipped = detail::tt_word::flip_all(cls.representative, mask, n) & detail::tt_word::full(n);
            for (const bool output : {false, true})
            {
                if ((output ? (~flipped & detail::tt_word::full(n)) : flipped) != word)
                {
                    continue;
                }
                if (const auto inversions = detail::inversions_after(*c, mask, output); inversions < best_cost)
                {
                    best_cost   = inversions;
                    best_mask   = mask;
                    best_output = output;
                }
            }
        }

        inst.cell = *c;
        detail::apply_negations(inst.cell, best_mask, best_output);
        assert(inst.cell.function() == word);

        return inst;
    }

  private:
    /**
     * Largest support size.
     */
    uint32_t k;
    /**
     * Per support size, the cells keyed by class representative.
     */
    std::vector<std::unordered_map<uint64_t, entry>> tables;
};

namespace detail
{

/**
 * Grows the planar structures of one support size and evaluates every gate and inversion assignment.
 */
class planar_cell_enumerator
{
  public:
    planar_cell_enumerator(const uint32_t num_inputs, const planar_cell_database_params& p,
                           planar_cell_database& database, planar_cell_database_stats::table_stats& stats) :
            k{num_inputs},
            ps{p},
            db{database},
            st{stats}
    {
        for (const auto g : ps.gate_set)
        {
            allowed.at(static_cast<std::size_t>(g)) = true;
        }
        st.num_inputs = k;
    }

    void run()
    {
        // the structures first, so that progress can be reported over them
        planar_cell skeleton{};
        skeleton.num_inputs = static_cast<uint8_t>(k);

        std::vector<uint8_t> front(k);
        for (uint32_t i = 0; i < k; ++i)
        {
            front[i] = static_cast<uint8_t>(i);
        }
        std::vector<uint8_t> fanout(k, 0);

        grow(skeleton, front, fanout, 0);

        st.structures = structures.size();

        utils::progress_reporter progress{ps.on_progress, "evaluating structures of " + std::to_string(k) + " inputs",
                                          structures.size()};
        for (const auto& s : structures)
        {
            evaluate(s);
            progress.advance();
        }

        // the classes of this support size, for the coverage statistic
        const auto full = tt_word::full(k);
        if (k <= 4)
        {
            std::vector<bool> seen(static_cast<std::size_t>(full) + 1, false);
            for (uint64_t f = 0; f <= full; ++f)
            {
                if (!tt_word::has_full_support(f, k))
                {
                    continue;
                }
                const auto rep = canonize(f, k).representative;
                if (!seen[static_cast<std::size_t>(rep)])
                {
                    seen[static_cast<std::size_t>(rep)] = true;
                    ++st.classes;
                }
            }
        }
        st.covered = db.num_cells(k);
    }

  private:
    /**
     * A group of neighbouring frontier positions that one new node reads.
     */
    using group = std::vector<uint8_t>;

    /**
     * Grows structures level by level. `front` holds the unconsumed signals left to right, `fanout` the consumer
     * count of every signal created so far, `level` the level of the newest nodes.
     */
    void grow(const planar_cell& cell, const std::vector<uint8_t>& front, const std::vector<uint8_t>& fanout,
              const uint32_t level)
    {
        // complete: exactly one signal without consumer, and it is a node of the newest level
        if (level > 0)
        {
            uint32_t open = 0;
            for (const auto s : front)
            {
                if (fanout[s] == 0)
                {
                    ++open;
                }
            }
            if (open == 1 && cell.nodes.back().level == level &&
                std::count_if(cell.nodes.begin(), cell.nodes.end(),
                              [level](const auto& n) { return n.level == level; }) == 1)
            {
                structures.push_back(cell);
                return;
            }
        }

        if (level + 1 == ps.max_levels)
        {
            // last level: join every signal without consumer
            group open{};
            for (const auto s : front)
            {
                if (fanout[s] == 0)
                {
                    open.push_back(s);
                }
            }
            if (open.size() == 2 || (open.size() == 3 && allowed[static_cast<std::size_t>(planar_gate::MAJ)]))
            {
                auto joined = cell;
                joined.nodes.push_back({open, static_cast<uint8_t>(level + 1), planar_gate::AND, 0});
                structures.push_back(std::move(joined));
            }
            return;
        }

        if (level + 1 > ps.max_levels)
        {
            return;
        }

        // every group of neighbouring positions a node may read, with at least one signal of the newest level
        std::vector<std::pair<uint8_t, uint8_t>> groups{};  // (first position, last position)
        std::vector<group>                       group_members{};
        const auto                               n = front.size();
        for (std::size_t i = 0; i < n; ++i)
        {
            for (std::size_t j = i + 1; j < n; ++j)
            {
                for (std::size_t l = j; l < n; ++l)
                {
                    if (l > j && !allowed[static_cast<std::size_t>(planar_gate::MAJ)])
                    {
                        break;
                    }
                    if (!enclosable(front, fanout, i, j) || (l > j && !enclosable(front, fanout, j, l)))
                    {
                        continue;
                    }
                    const auto newest = [&](const std::size_t p) { return level_of(cell, front[p]) == level; };
                    if (!newest(i) && !newest(j) && !newest(l))
                    {
                        continue;
                    }
                    groups.emplace_back(static_cast<uint8_t>(i), static_cast<uint8_t>(l));
                    group_members.push_back(l > j ? group{front[i], front[j], front[l]} : group{front[i], front[j]});
                }
            }
        }

        // every non-crossing combination of groups: left to right, the next group starts at or after the end of
        // the previous one; a signal is read by at most two nodes in total
        std::vector<std::size_t> chosen{};
        combine(cell, front, fanout, level, groups, group_members, 0, chosen);
    }

    /**
     * Whether the frontier positions strictly between `a` and `b` can be enclosed by an edge pair: all of them have
     * exactly one consumer already.
     */
    [[nodiscard]] static bool enclosable(const std::vector<uint8_t>& front, const std::vector<uint8_t>& fanout,
                                         const std::size_t a, const std::size_t b) noexcept
    {
        for (auto p = a + 1; p < b; ++p)
        {
            if (fanout[front[p]] != 1)
            {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] uint32_t level_of(const planar_cell& cell, const uint8_t signal) const noexcept
    {
        return signal < k ? 0u : cell.nodes[signal - k].level;
    }

    void combine(const planar_cell& cell, const std::vector<uint8_t>& front, const std::vector<uint8_t>& fanout,
                 const uint32_t level, const std::vector<std::pair<uint8_t, uint8_t>>& groups,
                 const std::vector<group>& group_members, const std::size_t from, std::vector<std::size_t>& chosen)
    {
        if (!chosen.empty())
        {
            place(cell, front, fanout, level, groups, group_members, chosen);
        }
        for (auto g = from; g < groups.size(); ++g)
        {
            if (!chosen.empty() && groups[g].first < groups[chosen.back()].second)
            {
                continue;
            }
            // fanout limit: a signal shared by two consecutive groups must have no consumer yet
            if (!chosen.empty() && groups[g].first == groups[chosen.back()].second &&
                fanout[front[groups[g].first]] != 0)
            {
                continue;
            }
            if (std::any_of(group_members[g].begin(), group_members[g].end(),
                            [&fanout](const auto s) { return fanout[s] >= 2; }))
            {
                continue;
            }
            chosen.push_back(g);
            combine(cell, front, fanout, level, groups, group_members, g + 1, chosen);
            chosen.pop_back();
        }
    }

    /**
     * Creates the nodes of one combination, builds the new frontier and recurses to the next level.
     */
    void place(const planar_cell& cell, const std::vector<uint8_t>& front, const std::vector<uint8_t>& fanout,
               const uint32_t level, const std::vector<std::pair<uint8_t, uint8_t>>& groups,
               const std::vector<group>& group_members, const std::vector<std::size_t>& chosen)
    {
        auto next        = cell;
        auto next_fanout = fanout;

        std::vector<uint8_t> new_nodes{};
        for (const auto g : chosen)
        {
            const auto node = static_cast<uint8_t>(k + next.nodes.size());
            next.nodes.push_back({group_members[g], static_cast<uint8_t>(level + 1), planar_gate::AND, 0});
            next_fanout.push_back(0);
            for (const auto s : group_members[g])
            {
                ++next_fanout[s];
            }
            new_nodes.push_back(node);
        }

        // new frontier: old signals with fewer than two consumers, enclosed ones dropped, new nodes after the first
        // fanin of their group
        std::vector<uint8_t> next_front{};
        std::size_t          gi = 0;
        std::size_t          p  = 0;
        while (p < front.size())
        {
            if (gi < chosen.size() && groups[chosen[gi]].first == p)
            {
                if (next_fanout[front[p]] < 2)
                {
                    next_front.push_back(front[p]);
                }
                next_front.push_back(new_nodes[gi]);
                p = groups[chosen[gi]].second;
                ++gi;
                if (gi < chosen.size() && groups[chosen[gi]].first == p)
                {
                    continue;
                }
            }
            if (next_fanout[front[p]] < 2)
            {
                next_front.push_back(front[p]);
            }
            ++p;
        }

        grow(next, next_front, next_fanout, level + 1);
    }

    /**
     * Assigns every allowed gate to every node and every inversion to the edges that canonization cannot absorb:
     * edges between nodes, and the second edge of an input read twice. Inversions on the other input edges and on
     * the output are reached by the negation class.
     */
    void evaluate(const planar_cell& skeleton)
    {
        // the inversion bits to enumerate, as (node, fanin) pairs
        std::vector<std::pair<std::size_t, std::size_t>> free_edges{};
        std::vector<uint8_t>                             input_seen(k, 0);
        for (std::size_t j = 0; j < skeleton.nodes.size(); ++j)
        {
            const auto& n = skeleton.nodes[j];
            for (std::size_t i = 0; i < n.fanins.size(); ++i)
            {
                if (n.fanins[i] >= k)
                {
                    free_edges.emplace_back(j, i);
                }
                else if (input_seen[n.fanins[i]]++ > 0)
                {
                    free_edges.emplace_back(j, i);
                }
            }
        }

        // the gate choices per node
        std::vector<std::vector<planar_gate>> choices(skeleton.nodes.size());
        for (std::size_t j = 0; j < skeleton.nodes.size(); ++j)
        {
            if (skeleton.nodes[j].fanins.size() == 3)
            {
                choices[j] = {planar_gate::MAJ};
                continue;
            }
            for (const auto g : {planar_gate::AND, planar_gate::OR, planar_gate::XOR})
            {
                if (allowed[static_cast<std::size_t>(g)])
                {
                    choices[j].push_back(g);
                }
            }
            if (choices[j].empty())
            {
                return;
            }
        }

        auto                     cell = skeleton;
        std::vector<std::size_t> pick(skeleton.nodes.size(), 0);
        while (true)
        {
            for (std::size_t j = 0; j < cell.nodes.size(); ++j)
            {
                cell.nodes[j].gate = choices[j][pick[j]];
            }
            for (uint64_t inv = 0; inv < (1ull << free_edges.size()); ++inv)
            {
                for (auto& n : cell.nodes)
                {
                    n.inverted_fanins = 0;
                }
                for (std::size_t e = 0; e < free_edges.size(); ++e)
                {
                    if (((inv >> e) & 1ull) != 0ull)
                    {
                        cell.nodes[free_edges[e].first].inverted_fanins |=
                            static_cast<uint8_t>(1u << free_edges[e].second);
                    }
                }
                ++st.evaluations;
                db.insert(cell);
            }

            // next gate assignment
            std::size_t j = 0;
            while (j < pick.size() && ++pick[j] == choices[j].size())
            {
                pick[j] = 0;
                ++j;
            }
            if (j == pick.size())
            {
                break;
            }
        }
    }

    uint32_t                                 k;
    const planar_cell_database_params&       ps;
    planar_cell_database&                    db;
    planar_cell_database_stats::table_stats& st;
    std::array<bool, 4>                      allowed{false, false, false, false};
    std::vector<planar_cell>                 structures{};
};

}  // namespace detail

/**
 * Enumerates planar cells for every support size from two to `ps.num_inputs` and collects the cheapest cell per
 * negation class in a database.
 *
 * Structures are grown level by level from the inputs with the rule of the owner's 2024 enumeration: a new node
 * reads two or three signals that neighbour each other on the frontier of unconsumed signals (signals in between are
 * allowed if they have exactly one consumer, which the new node's edges then enclose), at least one of them from the
 * newest level; a signal is read by at most two nodes; the last level joins every signal still without consumer. The
 * structure is complete when exactly one signal has no consumer. For every structure, every allowed gate per node
 * and every inversion on the edges between nodes is evaluated; inversions on input edges and on the output are
 * covered by the negation classes, so they need not be enumerated.
 *
 * @param ps Parameters.
 * @param pst Statistics.
 * @return The database.
 */
[[nodiscard]] inline planar_cell_database generate_planar_cells(const planar_cell_database_params& ps  = {},
                                                                planar_cell_database_stats*        pst = nullptr)
{
    assert(ps.num_inputs >= 2 && ps.num_inputs <= 6);

    planar_cell_database_stats stats{};
    planar_cell_database       db{ps.num_inputs};
    {
        const mockturtle::stopwatch stop{stats.time_total};
        for (uint32_t n = 2; n <= ps.num_inputs; ++n)
        {
            stats.tables.emplace_back();
            detail::planar_cell_enumerator e{n, ps, db, stats.tables.back()};
            e.run();
        }
    }
    if (pst != nullptr)
    {
        *pst = stats;
    }
    return db;
}

}  // namespace fiction::synthesis
