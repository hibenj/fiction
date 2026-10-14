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
 * @brief Replaces every LUT of a ranked, balanced network by its planar cell, keeping the ranks and the crossings.
 * @author Benjamin Hien (hibenj)
 */

#pragma once

#include "fiction/networks/technology_network.hpp"
#include "fiction/networks/views/mutable_rank_view.hpp"
#include "fiction/synthesis/planar_cell_database.hpp"
#include "fiction/utils/progress.hpp"

#include <kitty/bit_operations.hpp>
#include <kitty/dynamic_truth_table.hpp>
#include <kitty/operations.hpp>
#include <mockturtle/traits.hpp>
#include <mockturtle/utils/node_map.hpp>
#include <mockturtle/utils/stopwatch.hpp>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace fiction::synthesis
{

/**
 * Parameters of the planar cell substitution.
 */
struct planar_cell_substitution_params
{
    /**
     * Reports completed work per level.
     */
    utils::progress_callback on_progress{};
};

/**
 * Statistics of the planar cell substitution.
 */
struct planar_cell_substitution_stats
{
    /**
     * LUTs of the input (nodes with two or more fanins).
     */
    uint64_t luts{0};
    /**
     * LUTs replaced by a cell.
     */
    uint64_t luts_with_cell{0};
    /**
     * LUTs kept because the database has no cell for their function in their fanin order.
     */
    uint64_t luts_kept{0};
    /**
     * Gates of the cells.
     */
    uint64_t gates{0};
    /**
     * Inverter nodes created for inverted cell edges.
     */
    uint64_t inverters{0};
    /**
     * Buffers created inside cells for edges that span more than one level, inverters included.
     */
    uint64_t cell_buffers{0};
    /**
     * Buffers that lift cells, kept LUTs and inverters of the input to the top of their column.
     */
    uint64_t padding{0};
    /**
     * Buffers that carry the input's buffer chains through the columns.
     */
    uint64_t chain_buffers{0};
    /**
     * Levels of the input, inputs not counted.
     */
    uint32_t levels_before{0};
    /**
     * Levels of the result, inputs not counted.
     */
    uint32_t levels_after{0};
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
        out << "[i] LUTs = " << luts << " (" << luts_with_cell << " replaced, " << luts_kept << " kept)\n";
        out << "[i] gates = " << gates << ", inverters = " << inverters << ", cell buffers = " << cell_buffers
            << ", padding = " << padding << ", chain buffers = " << chain_buffers << '\n';
        out << "[i] levels = " << levels_before << " -> " << levels_after << '\n';
        out << "[i] total time = " << mockturtle::to_seconds(time_total) << " s\n";
    }
};

namespace detail
{

/**
 * Permutes the variables of `f`: variable `i` of the result is variable `order[i]` of `f`.
 */
[[nodiscard]] inline kitty::dynamic_truth_table permute_variables(const kitty::dynamic_truth_table& f,
                                                                  const std::vector<uint8_t>&       order)
{
    kitty::dynamic_truth_table r{f.num_vars()};
    for (uint64_t m = 0; m < r.num_bits(); ++m)
    {
        uint64_t src = 0;
        for (std::size_t i = 0; i < order.size(); ++i)
        {
            src |= ((m >> i) & 1ull) << order[i];
        }
        if (kitty::get_bit(f, src))
        {
            kitty::set_bit(r, m);
        }
    }
    return r;
}

template <typename Ntk, typename = void>
struct has_foreach_pi_unranked : std::false_type
{};

template <typename Ntk>
struct has_foreach_pi_unranked<
    Ntk, std::void_t<decltype(std::declval<const Ntk&>().foreach_pi_unranked(std::declval<void (*)(int)>()))>>
        : std::true_type
{};

template <typename Ntk>
inline constexpr bool has_foreach_pi_unranked_v = has_foreach_pi_unranked<Ntk>::value;

/**
 * Implementation of the planar cell substitution.
 *
 * @tparam Ntk Ranked network type.
 */
template <typename Ntk>
class planar_cell_substitution_impl
{
  public:
    /**
     * Node type of the input.
     */
    using node = mockturtle::node<Ntk>;
    /**
     * Type of the result network.
     */
    using out_nt = networks::technology_network;
    /**
     * Signal type of the result network.
     */
    using out_sg = out_nt::signal;
    /**
     * Node type of the result network.
     */
    using out_nd = out_nt::node;

    /**
     * Creates the implementation.
     *
     * @param src Input network.
     * @param database Cell database.
     * @param p Parameters.
     * @param st Statistics to fill.
     */
    planar_cell_substitution_impl(const Ntk& src, const planar_cell_database& database,
                                  planar_cell_substitution_params p, planar_cell_substitution_stats& st) :
            ntk{src},
            db{database},
            ps{std::move(p)},
            pst{st}
    {}

    /**
     * Runs the substitution.
     *
     * @return The ranked result.
     */
    [[nodiscard]] networks::views::mutable_rank_view<out_nt> run()
    {
        const mockturtle::stopwatch stop{pst.time_total};

        const auto depth  = ntk.depth();
        pst.levels_before = depth;

        // every node's plan: its cell with the negations folded in, or none, and the depth of its column share
        plan_level_zero();
        std::vector<uint32_t> column_depth(depth + 1, 1);
        plans.resize(depth + 1);
        for (uint32_t level = 1; level <= depth; ++level)
        {
            ntk.foreach_node_in_rank(level,
                                     [this, level, &column_depth](const auto& n)
                                     {
                                         auto p              = plan(n);
                                         column_depth[level] = std::max(column_depth[level], p.depth);
                                         plans[level].push_back(std::move(p));
                                     });
        }

        // the output level of every input level's results
        top.assign(depth + 1, 0);
        for (uint32_t level = 1; level <= depth; ++level)
        {
            top[level] = top[level - 1] + column_depth[level];
        }
        pst.levels_after = top[depth];
        out_ranks.assign(top[depth] + 1, {});

        // inputs in the input's order, ranked in its rank order
        // a rank view iterates its inputs in rank order; the result's inputs must follow the input's own order so that
        // input `i` of both networks is the same
        if constexpr (has_foreach_pi_unranked_v<Ntk>)
        {
            ntk.foreach_pi_unranked([this](const auto& n) { signal_of[n] = out.create_pi(); });
        }
        else
        {
            ntk.foreach_pi([this](const auto& n) { signal_of[n] = out.create_pi(); });
        }
        ntk.foreach_node_in_rank(0,
                                 [this](const auto& n)
                                 {
                                     if (!ntk.is_constant(n))
                                     {
                                         out_ranks[0].push_back(out.get_node(signal_of[n]));
                                     }
                                 });

        utils::progress_reporter progress{ps.on_progress, "substituting cells", depth};
        for (uint32_t level = 1; level <= depth; ++level)
        {
            for (auto& p : plans[level])
            {
                build(p, level, column_depth[level]);
            }
            progress.advance();
        }

        // outputs as in the input
        ntk.foreach_po(
            [this](const auto& po)
            {
                const auto n = ntk.get_node(po);
                if (ntk.is_constant(n))
                {
                    out.create_po(out.get_constant(ntk.constant_value(n)));
                    return;
                }
                out.create_po(signal_of[n]);
            });

        return networks::views::mutable_rank_view<out_nt>{out, out_ranks};
    }

  private:
    /**
     * What one input node becomes.
     */
    struct node_plan
    {
        node                                          n{};
        std::vector<node>                             fanins{};    // in rank order of the level below
        std::optional<planar_cell_database::instance> cell{};      // the cell in that order, or none
        kitty::dynamic_truth_table                    function{};  // the function in that order (kept LUTs)
        std::vector<uint32_t>                         rel{};       // column level of every cell signal
        std::vector<uint32_t>                         level_of{};  // column level of every cell level
        std::vector<uint32_t>                         x{};         // left-to-right position of every cell signal
        uint32_t                                      depth{1};    // levels the node occupies in its column
    };

    void plan_level_zero()
    {
        ntk.foreach_node_in_rank(0,
                                 [this](const auto& n)
                                 {
                                     if (!ntk.is_constant(n) && !ntk.is_ci(n))
                                     {
                                         throw std::invalid_argument(
                                             "the input network must have only inputs on level 0");
                                     }
                                 });
    }

    [[nodiscard]] node_plan plan(const node& n)
    {
        node_plan p{};
        p.n = n;
        ntk.foreach_fanin(n,
                          [this, &p](const auto& f)
                          {
                              const auto fn = ntk.get_node(f);
                              if (ntk.is_constant(fn))
                              {
                                  throw std::invalid_argument("constant fanins are not supported");
                              }
                              p.fanins.push_back(fn);
                          });

        // buffers (fanouts included) and inverters pass through with depth 1
        if (p.fanins.size() < 2)
        {
            return p;
        }

        ++pst.luts;

        // the fanins in the rank order of the level below, and the function in that order
        std::vector<uint8_t> order(p.fanins.size());
        for (std::size_t i = 0; i < order.size(); ++i)
        {
            order[i] = static_cast<uint8_t>(i);
        }
        std::sort(order.begin(), order.end(), [this, &p](const auto a, const auto b)
                  { return ntk.rank_position(p.fanins[a]) < ntk.rank_position(p.fanins[b]); });
        std::vector<node> sorted{};
        sorted.reserve(p.fanins.size());
        for (const auto i : order)
        {
            sorted.push_back(p.fanins[i]);
        }
        auto f = ntk.node_function(n);
        if (f.num_vars() > p.fanins.size())
        {
            f = kitty::shrink_to(f, static_cast<uint32_t>(p.fanins.size()));
        }
        p.function = permute_variables(f, order);
        p.fanins   = std::move(sorted);

        p.cell = db.lookup(p.function);
        if (!p.cell.has_value())
        {
            ++pst.luts_kept;
            return p;
        }
        ++pst.luts_with_cell;

        const auto& cell = p.cell->cell;
        const auto  k    = static_cast<uint32_t>(cell.num_inputs);

        // an input the cell reads twice enters through one fanout node on the first level, so that the column has
        // one edge per fanin like the LUT and the crossings at its boundary stay the LUT's
        std::vector<uint32_t> reads(k, 0);
        for (const auto& cn : cell.nodes)
        {
            for (const auto fi : cn.fanins)
            {
                if (fi < k)
                {
                    ++reads[fi];
                }
            }
        }
        p.rel.assign(k + cell.nodes.size(), 0);
        for (uint32_t i = 0; i < k; ++i)
        {
            p.rel[i] = reads[i] > 1 ? 1u : 0u;
        }

        // the cell's levels are stretched, never reordered: every node of a cell level sits on the same column
        // level, which is one above the highest arrival among the level's fanin edges (an inverted edge arrives one
        // level later, because its inverter is a node), so that the planar drawing of the enumeration survives
        const auto cell_depth = cell.depth();
        p.level_of.assign(cell_depth + 1, 0);
        for (uint32_t lvl = 1; lvl <= cell_depth; ++lvl)
        {
            p.level_of[lvl] = p.level_of[lvl - 1] + 1;
            for (std::size_t j = 0; j < cell.nodes.size(); ++j)
            {
                const auto& cn = cell.nodes[j];
                if (cn.level != lvl)
                {
                    continue;
                }
                for (std::size_t i = 0; i < cn.fanins.size(); ++i)
                {
                    const bool inverted = ((static_cast<uint32_t>(cn.inverted_fanins) >> i) & 1u) != 0u;
                    p.level_of[lvl]     = std::max(p.level_of[lvl], p.rel[cn.fanins[i]] + 1u + (inverted ? 1u : 0u));
                }
            }
            for (std::size_t j = 0; j < cell.nodes.size(); ++j)
            {
                if (cell.nodes[j].level == lvl)
                {
                    p.rel[k + j] = p.level_of[lvl];
                }
            }
        }

        // the left-to-right order of the enumeration: every node sits right before its last fanin
        std::vector<uint32_t> order_of_signals{};
        for (uint32_t i = 0; i < k; ++i)
        {
            order_of_signals.push_back(i);
        }
        for (std::size_t j = 0; j < cell.nodes.size(); ++j)
        {
            const auto last = std::find(order_of_signals.begin(), order_of_signals.end(),
                                        static_cast<uint32_t>(cell.nodes[j].fanins.back()));
            order_of_signals.insert(last, static_cast<uint32_t>(k + j));
        }
        p.x.assign(k + cell.nodes.size(), 0);
        for (uint32_t i = 0; i < order_of_signals.size(); ++i)
        {
            p.x[order_of_signals[i]] = i;
        }

        p.depth = cell.nodes.empty() ? 1u : p.rel.back() + (cell.output_inverted ? 1u : 0u);
        p.depth = std::max(p.depth, 1u);

        return p;
    }

    /**
     * An item of a column level: a node of the result with its left-to-right key. A gate is keyed by its own
     * position. An item of an edge that passes the level is keyed by the position of the edge's source; among
     * the edges of one source, the ones that end to the right of it come after the ones that end to its left, and
     * on each side the edge that ends first is the outermost, so that no edge of the fan crosses another one when
     * it bends towards its target.
     */
    struct item
    {
        int32_t x{0};
        int32_t side{0};
        int32_t bend{0};
        out_nd  n{};
    };

    void build(node_plan& p, const uint32_t level, const uint32_t column_depth)
    {
        const auto base = top[level - 1];  // the output level of the fanins

        std::vector<std::vector<item>> items(column_depth + 1);  // index = relative level 1..column_depth
        const auto                     place_gate = [&items](const uint32_t rel, const uint32_t x, const out_nd n)
        { items[rel].push_back({static_cast<int32_t>(x), 0, 0, n}); };
        const auto place_edge =
            [&items](const uint32_t rel, const uint32_t xs, const uint32_t xt, const uint32_t tgt_level, const out_nd n)
        {
            const int32_t side = xt > xs ? 1 : -1;
            items[rel].push_back({static_cast<int32_t>(xs), side, -side * static_cast<int32_t>(tgt_level), n});
        };

        out_sg   result{};
        uint32_t result_rel = 1;

        if (!p.cell.has_value())
        {
            // a buffer, an inverter or a kept LUT: one node, then padding
            std::vector<out_sg> fanins{};
            for (const auto& f : p.fanins)
            {
                fanins.push_back(signal_of[f]);
            }
            const bool chain = p.fanins.size() < 2 && !ntk.is_inv(p.n);
            if (p.fanins.size() >= 2)
            {
                result = out.create_node(fanins, p.function);
            }
            else if (ntk.is_inv(p.n))
            {
                result = out.create_not(fanins.front());
            }
            else
            {
                result = out.create_buf(fanins.front());
                ++pst.chain_buffers;
            }
            place_gate(1, 0, out.get_node(result));
            for (uint32_t r = 2; r <= column_depth; ++r)
            {
                result = out.create_buf(result);
                place_gate(r, 0, out.get_node(result));
                if (chain)
                {
                    ++pst.chain_buffers;
                }
                else
                {
                    ++pst.padding;
                }
            }
        }
        else
        {
            const auto& inst = *p.cell;
            const auto& cell = inst.cell;
            const auto  k    = static_cast<uint32_t>(cell.num_inputs);

            // the cell's inputs: its support in rank order, through a fanout node where the cell reads one twice
            std::vector<out_sg> sig(k + cell.nodes.size());
            for (uint32_t i = 0; i < k; ++i)
            {
                sig[i] = signal_of[p.fanins[inst.support[i]]];
                if (p.rel[i] == 1)
                {
                    sig[i] = out.create_buf(sig[i]);
                    ++pst.cell_buffers;
                    place_gate(1, p.x[i], out.get_node(sig[i]));
                }
            }

            for (std::size_t j = 0; j < cell.nodes.size(); ++j)
            {
                const auto&         cn  = cell.nodes[j];
                const auto          xj  = p.x[k + j];
                const auto          lvl = static_cast<uint32_t>(cn.level);
                std::vector<out_sg> fanins{};
                for (std::size_t i = 0; i < cn.fanins.size(); ++i)
                {
                    const auto src      = cn.fanins[i];
                    const bool inverted = ((static_cast<uint32_t>(cn.inverted_fanins) >> i) & 1u) != 0u;
                    auto       s        = sig[src];
                    auto       r        = p.rel[src];
                    if (inverted)
                    {
                        s = out.create_not(s);
                        ++r;
                        ++pst.inverters;
                        place_edge(r, p.x[src], xj, lvl, out.get_node(s));
                    }
                    while (r + 1 < p.rel[k + j])
                    {
                        s = out.create_buf(s);
                        ++r;
                        ++pst.cell_buffers;
                        place_edge(r, p.x[src], xj, lvl, out.get_node(s));
                    }
                    fanins.push_back(s);
                }
                out_sg s{};
                switch (cn.gate)
                {
                    case planar_gate::AND:
                    {
                        s = out.create_and(fanins[0], fanins[1]);
                        break;
                    }
                    case planar_gate::OR:
                    {
                        s = out.create_or(fanins[0], fanins[1]);
                        break;
                    }
                    case planar_gate::XOR:
                    {
                        s = out.create_xor(fanins[0], fanins[1]);
                        break;
                    }
                    case planar_gate::MAJ:
                    {
                        s = out.create_maj(fanins[0], fanins[1], fanins[2]);
                        break;
                    }
                }
                ++pst.gates;
                sig[k + j] = s;
                place_gate(p.rel[k + j], xj, out.get_node(s));
            }

            uint32_t xr = 0;
            if (cell.nodes.empty())
            {
                // a single variable: a buffer or an inverter of it
                result = cell.output_inverted ? out.create_not(sig[0]) : out.create_buf(sig[0]);
                if (cell.output_inverted)
                {
                    ++pst.inverters;
                }
                else
                {
                    ++pst.padding;
                }
                place_gate(1, 0, out.get_node(result));
                result_rel = 1;
            }
            else
            {
                result     = sig.back();
                result_rel = p.rel.back();
                xr         = p.x.back();
                if (cell.output_inverted)
                {
                    result = out.create_not(result);
                    ++result_rel;
                    ++pst.inverters;
                    place_gate(result_rel, xr, out.get_node(result));
                }
            }
            for (auto r = result_rel + 1; r <= column_depth; ++r)
            {
                result = out.create_buf(result);
                ++pst.padding;
                place_gate(r, xr, out.get_node(result));
            }
        }

        signal_of[p.n] = result;

        // the column's nodes in left-to-right order, level by level
        for (uint32_t r = 1; r <= column_depth; ++r)
        {
            auto& level_items = items[r];
            std::stable_sort(level_items.begin(), level_items.end(), [](const item& a, const item& b)
                             { return std::make_tuple(a.x, a.side, a.bend) < std::make_tuple(b.x, b.side, b.bend); });
            for (const auto& it : level_items)
            {
                out_ranks[base + r].push_back(it.n);
            }
        }
    }

    const Ntk&                            ntk;
    const planar_cell_database&           db;
    const planar_cell_substitution_params ps;
    planar_cell_substitution_stats&       pst;

    out_nt                              out{};
    mockturtle::node_map<out_sg, Ntk>   signal_of{ntk};
    std::vector<std::vector<node_plan>> plans{};
    std::vector<uint32_t>               top{};
    std::vector<std::vector<out_nd>>    out_ranks{};
};

}  // namespace detail

/**
 * Replaces every LUT of a ranked, balanced network by its planar cell. Every level of the input becomes a column
 * of as many levels as its deepest cell needs; a cell is built from its LUT's fanins in the rank order of the level
 * below (the database holds cells per negation class in a fixed input order), its gates sit on the column's levels,
 * every inverted edge is an inverter node on its own level, an input the cell reads twice enters through one fanout
 * node, edges that span levels get buffers, and shallower
 * cells, kept LUTs, inverters and buffer chains of the input are lifted to the top of the column with buffers. The
 * rank order of every result level follows the input's rank order and, inside a cell, the left-to-right order of
 * the cell's enumeration, so the result has exactly the crossings of the input: the cells add none. A LUT whose
 * function has no cell in its fanin order is kept as it is (`planar_cell_substitution_stats::luts_kept`).
 *
 * The outputs are left where the input has them; unify them before the substitution if a later stage needs that.
 *
 * @tparam Ntk Ranked network type (see `mutable_rank_view`) over a network with LUT functions, buffers and inverters,
 * balanced, inputs on level 0, no constant fanins.
 * @param ntk The ranked, balanced input network.
 * @param db The cell database; its support size must cover the LUTs' fanin counts.
 * @param ps Parameters.
 * @param pst Statistics.
 * @return The ranked, balanced result with the input's crossings.
 */
template <typename Ntk>
[[nodiscard]] networks::views::mutable_rank_view<networks::technology_network>
planar_cell_substitution(const Ntk& ntk, const planar_cell_database& db, const planar_cell_substitution_params& ps = {},
                         planar_cell_substitution_stats* pst = nullptr)
{
    static_assert(mockturtle::is_network_type_v<Ntk>, "Ntk is not a network type");
    static_assert(mockturtle::has_rank_position_v<Ntk>, "Ntk does not implement the rank_position function");
    static_assert(mockturtle::has_foreach_node_in_rank_v<Ntk>, "Ntk does not implement foreach_node_in_rank");
    static_assert(mockturtle::has_node_function_v<Ntk>, "Ntk does not implement the node_function function");
    static_assert(mockturtle::has_depth_v<Ntk>, "Ntk does not implement the depth function");
    static_assert(mockturtle::has_is_buf_v<Ntk>, "Ntk does not implement the is_buf function");

    planar_cell_substitution_stats             st{};
    detail::planar_cell_substitution_impl<Ntk> impl{ntk, db, ps, st};
    auto                                       result = impl.run();
    if (pst != nullptr)
    {
        *pst = st;
    }
    return result;
}

}  // namespace fiction::synthesis
