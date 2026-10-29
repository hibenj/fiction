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
 * @brief Planar placement and routing by gap propagation, the reference predecessor of PLANE.
 * @author Benjamin Hien (hibenj)
 */

#pragma once

#include "fiction/layouts/cartesian_layout.hpp"
#include "fiction/layouts/clocking_scheme.hpp"
#include "fiction/layouts/layout_base.hpp"
#include "fiction/networks/name_utils.hpp"
#include "fiction/networks/network_utils.hpp"
#include "fiction/networks/technology_network.hpp"
#include "fiction/physical_design/placement_utils.hpp"
#include "fiction/physical_design/plane.hpp"
#include "fiction/types.hpp"

#include <fmt/format.h>
#include <mockturtle/traits.hpp>
#include <mockturtle/utils/node_map.hpp>
#include <mockturtle/utils/stopwatch.hpp>
#include <mockturtle/views/fanout_view.hpp>

#include <algorithm>
#include <compare>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <map>
#include <numeric>
#include <optional>
#include <stdexcept>
#include <vector>

namespace fiction::physical_design
{

/**
 * @brief Parameters of the gap-propagation placement.
 */
struct orthogonal_planar_params
{
    /**
     * @brief Number of clock phases of the 2DDWave clocking scheme.
     *
     * Three and four are supported.
     */
    layouts::clocking::num_clks number_of_clock_phases = layouts::clocking::num_clks::FOUR;
    /**
     * @brief Largest number of propagation rounds before the algorithm gives up.
     *
     * Every round raises at least one root gap by at least one tile, and no demand depends on a level's shift, so the
     * rounds are bounded by the number of shrinking sites along the chains; a round costs one pass over the network.
     * The budget is a safety net; large networks with long chains need thousands of rounds.
     */
    uint32_t max_rounds = 65536;
};
/**
 * @brief Statistics of the gap-propagation placement.
 */
struct orthogonal_planar_stats : plane_stats
{
    /**
     * @brief Propagation rounds the placement needed; one when no gap had to be raised.
     */
    uint32_t rounds{0};
    /**
     * @brief Writes the statistics to a stream.
     *
     * @param out Stream to write to.
     */
    void report(std::ostream& out = std::cout) const
    {
        plane_stats::report(out);
        out << fmt::format("[i] rounds          = {}\n", rounds);
    }
};

namespace detail
{

/**
 * @brief Implementation of the gap-propagation placement.
 *
 * The placement keeps the diagonal model of PLANE: level `l` of the ranked network occupies one anti-diagonal of a
 * 2DDWave-clocked layout, rank position 0 at its top right end. Unlike PLANE, every wire of a level runs east by the
 * level's routing lines, and a single-fanin node sits either east of its wire end or south of it; the free tiles
 * between rank neighbours, the gaps, are the state of the algorithm.
 *
 * Gaps form chains. The gap between two rank neighbours descends from the gap between their predecessors, unless the
 * two nodes are the consumers of one fanout node (a fanout root) or primary inputs (a PI root). A gate consumes the gap
 * between its two fanins. The gap between two nodes equals the parent gap plus the difference of the two nodes' south
 * offsets from their predecessors, so a node south of its wire end left of a node east of its wire end shrinks a gap
 * by one tile, and a gate whose fanins are further apart than one tile pushes the whole level further east.
 *
 * A negative gap is a collision. The algorithm places the roots at zero, computes every gap level by level, and raises
 * the root of every negative gap's chain by the shortfall: a PI root by spreading the inputs, a fanout root by placing
 * the fanout node's south consumer further south. It repeats until no gap is negative or the round budget is exhausted.
 * Then it places the nodes at the computed positions through the routing helpers of `plane.hpp`.
 *
 * @tparam Lyt Gate-level layout type.
 * @tparam Ntk Ranked, planar network type.
 */
template <typename Lyt, typename Ntk>
class orthogonal_planar_impl
{
  public:
    /**
     * @brief Input network wrapped in a fanout view.
     */
    using fanout_ntk = mockturtle::fanout_view<Ntk>;
    /**
     * @brief Node type.
     */
    using node = mockturtle::node<fanout_ntk>;
    /**
     * @brief Coordinate type.
     */
    using coordinate = layouts::layout_base::coordinate;
    /**
     * @brief Creates the implementation.
     *
     * @param src Network.
     * @param p Parameters.
     * @param st Statistics.
     */
    orthogonal_planar_impl(const Ntk& src, const orthogonal_planar_params& p, orthogonal_planar_stats& st) :
            ntk{src},
            placement_ntk{placement_view(ntk)},
            ps{p},
            pst{st},
            node2pos{ntk},
            layout{{0, 0}, layouts::clocking::twoddwave(ps.number_of_clock_phases)},
            levels(ntk.depth() + 1),
            pi_roots(ntk.rank_width(0) > 0 ? ntk.rank_width(0) - 1 : 0, 0)
    {
        build_chains();
    }
    /**
     * @brief Places and routes the network.
     *
     * @return The layout.
     * @throws std::runtime_error If the gaps diverge or do not settle within the round budget.
     * @throws std::logic_error If the placement breaks the diagonal invariant or a wire runs into an occupied tile.
     */
    [[nodiscard]] Lyt run()
    {
        const mockturtle::stopwatch stop{pst.time_total};

        // a root larger than this cannot come from a consistent set of demands: every gap of a settled placement is
        // at most the sum of the shifts of the levels above it, and every shift is at most one more than the number of
        // nodes of its level. A root beyond the bound is being driven by a demand that feeds back into itself.
        const auto divergence_bound = static_cast<uint64_t>(ntk.size()) * (static_cast<uint64_t>(ntk.depth()) + 1);

        bool settled = false;
        for (uint32_t round = 0; round < ps.max_rounds && !settled; ++round)
        {
            settled    = forward_pass();
            pst.rounds = round + 1;

            uint64_t largest_root = pi_roots.empty() ? uint64_t{0} : std::ranges::max(pi_roots);
            for (const auto& level : levels)
            {
                for (const auto d : level.fanout_roots)
                {
                    largest_root = std::max(largest_root, d);
                }
            }

            if (largest_root > divergence_bound)
            {
                throw std::runtime_error(fmt::format(
                    "gap propagation diverged after {} rounds: a demand feeds back into the level that raised it",
                    pst.rounds));
            }
        }

        if (!settled)
        {
            throw std::runtime_error(fmt::format("gap propagation did not settle within {} rounds", ps.max_rounds));
        }

        // the storage is sparse, so the working area can be generous: inputs plus their spread bound the first
        // diagonal, the shifts bound the advance, the outputs add rows. Shrunk to the bounding box at the end.
        const auto spread = std::accumulate(pi_roots.cbegin(), pi_roots.cend(), uint64_t{0});
        const auto advance =
            std::accumulate(levels.cbegin(), levels.cend(), uint64_t{0},
                            [](const uint64_t sum, const level_data& level) { return sum + level.shift; });
        const auto bound = static_cast<uint64_t>(ntk.num_pis()) + spread + advance + ntk.num_pos() + 2;
        layout.resize({bound, bound});

        for (uint32_t lvl = 0; lvl <= ntk.depth(); ++lvl)
        {
            place_level(lvl);
        }

        const auto extra_rows = route_outputs(layout, ntk, node2pos, first_pos, place_t);

        layout.resize({static_cast<uint64_t>(first_pos.x) + 2, static_cast<uint64_t>(place_t.y) + extra_rows + 1, 1});

        // the inputs were created in rank order; the layout lists them in the order of the network
        std::vector<typename Lyt::object_id> input_order{};
        input_order.reserve(ntk.num_pis());
        ntk.foreach_pi([this, &input_order](const auto& pi) { input_order.push_back(pi2node[pi]); });
        layout.set_input_order(input_order);

        networks::restore_names(ntk, layout, node2pos);

        pst.x_size    = layout.width();
        pst.y_size    = layout.height();
        pst.num_gates = layout.num_gates();
        pst.num_wires = layout.num_wires();

        return layout;
    }

  private:
    /**
     * @brief Kind of a gap, as far as the propagation is concerned.
     */
    enum class gap_kind : uint8_t
    {
        /**
         * @brief Between two primary inputs; a free root.
         */
        PI_ROOT,
        /**
         * @brief Between the two consumers of one fanout node whose south consumer is a single-fanin node; a free root,
         * raised by placing the south consumer further south.
         */
        FANOUT_ROOT,
        /**
         * @brief Between the two consumers of one fanout node whose south consumer is a gate; its value is that gate's
         * inner gap, so a demand on it travels on to the inner gap's chain.
         */
        FORCED_ROOT,
        /**
         * @brief Descends from the gap between the two predecessors.
         */
        CHAINED
    };
    /**
     * @brief Wire of a node from its predecessor, as the placement runs it.
     *
     * A single-fanin node's wire runs `east` then `south` and the node sits east or south of the wire end; its south
     * offset `dy` from the predecessor is `south` plus one if it sits south. A gate's north fanin wire runs `east` and
     * then straight south to the gate, its west fanin wire runs straight east.
     */
    struct node_wiring
    {
        /**
         * @brief East run of the wire; the north fanin's for a gate.
         */
        uint64_t east{0};
        /**
         * @brief South run of the wire before the node; zero for a gate.
         */
        uint64_t south{0};
        /**
         * @brief Whether a single-fanin node sits south of its wire end instead of east.
         */
        bool south_of_wire{false};
        /**
         * @brief South offset of the node from the predecessor that bounds its left gap.
         */
        uint64_t dy_left{0};
        /**
         * @brief South offset of the node from the predecessor that bounds its right gap.
         */
        uint64_t dy_right{0};
    };
    /**
     * @brief Role of a node in the wiring of its level.
     */
    enum class node_role : uint8_t
    {
        /**
         * @brief Two-input gate.
         */
        GATE,
        /**
         * @brief Single-fanin node whose predecessor drives two nodes; this is the one with the smaller rank position.
         */
        EAST_CONSUMER,
        /**
         * @brief Single-fanin node whose predecessor drives two nodes; this is the one with the larger rank position.
         */
        SOUTH_CONSUMER,
        /**
         * @brief Any other single-fanin node, or a primary input.
         */
        FREE
    };
    /**
     * @brief Static facts about a node, computed once.
     */
    struct node_info
    {
        /**
         * @brief Role.
         */
        node_role role{node_role::FREE};
        /**
         * @brief Rank position of the leftmost predecessor on the level above: the north fanin of a gate, the only
         * fanin otherwise.
         */
        std::size_t left_pre{0};
        /**
         * @brief Rank position of the rightmost predecessor: the west fanin of a gate, the only fanin otherwise.
         */
        std::size_t right_pre{0};
    };
    /**
     * @brief Per-level state.
     */
    struct level_data
    {
        /**
         * @brief Static facts about every node of the level, in rank order.
         */
        std::vector<node_info> nodes;
        /**
         * @brief Gap right of every rank position but the last; negative while a round is unsettled.
         */
        std::vector<int64_t> gaps;
        /**
         * @brief Kind of every gap.
         */
        std::vector<gap_kind> kinds;
        /**
         * @brief Whether the chain of every gap ends at a PI root.
         */
        std::vector<bool> pi_rooted;
        /**
         * @brief Value demanded of every fanout root of the level; unused entries of the other kinds stay zero.
         *
         * The south consumer of the fanout node sits this many tiles further south, so the gap between the two
         * consumers equals the value.
         */
        std::vector<uint64_t> fanout_roots;
        /**
         * @brief Shift of the level: diagonal steps from the level above.
         */
        uint64_t shift{1};
        /**
         * @brief Wiring of every node of the level.
         */
        std::vector<node_wiring> wiring;
    };
    /**
     * @brief Builds the chain structure: the kind of every gap and whether its chain ends at a PI root.
     *
     * The gap between rank neighbours `u'` and `v'` on level `l` descends from the gap between `u'`'s rightmost
     * predecessor and `v'`'s leftmost one, which are rank neighbours on level `l - 1` in a planar, balanced network.
     *
     * @throws std::logic_error If two rank neighbours' predecessors are not rank neighbours, which a planar, balanced
     * network never produces.
     */
    void build_chains()
    {
        for (uint32_t lvl = 0; lvl <= ntk.depth(); ++lvl)
        {
            auto&      level = levels[lvl];
            const auto width = static_cast<std::size_t>(ntk.rank_width(lvl));
            const auto gaps  = width > 0 ? width - 1 : 0;

            level.nodes.assign(width, node_info{});
            level.gaps.assign(gaps, 0);
            level.kinds.assign(gaps, gap_kind::CHAINED);
            level.pi_rooted.assign(gaps, false);
            level.fanout_roots.assign(gaps, 0);
            level.wiring.assign(width, node_wiring{});

            if (lvl == 0)
            {
                level.kinds.assign(gaps, gap_kind::PI_ROOT);
                level.pi_rooted.assign(gaps, true);
                continue;
            }

            for (std::size_t r = 0; r < width; ++r)
            {
                const auto n      = ntk.at_rank_position(lvl, static_cast<uint32_t>(r));
                const auto fanins = ranked_fanins(ntk, n);
                auto&      info   = level.nodes[r];

                info.left_pre  = static_cast<std::size_t>(ntk.rank_position(fanins.front()));
                info.right_pre = static_cast<std::size_t>(ntk.rank_position(fanins.back()));

                if (fanins.size() == MAX_DEGREE)
                {
                    info.role = node_role::GATE;
                }
                else if (ntk.fanout_size(fanins.front()) == MAX_DEGREE)
                {
                    info.role = ranked_fanouts(ntk, fanins.front()).front() == n ? node_role::EAST_CONSUMER :
                                                                                   node_role::SOUTH_CONSUMER;
                }
            }

            for (std::size_t i = 0; i < gaps; ++i)
            {
                const auto& left  = level.nodes[i];
                const auto& right = level.nodes[i + 1];

                if (left.right_pre == right.left_pre)
                {
                    // the gap between the two consumers of one node is the south consumer's south offset minus the
                    // east consumer's, minus one: the fanout root when the south consumer is a single-fanin node (an
                    // east gate contributes offset zero either way), the south gate's inner gap otherwise
                    level.kinds[i] = right.role == node_role::GATE ? gap_kind::FORCED_ROOT : gap_kind::FANOUT_ROOT;
                    continue;
                }

                if (right.left_pre != left.right_pre + 1)
                {
                    throw std::logic_error(fmt::format(
                        "the predecessors of the rank neighbours at positions {} and {} on level {} are not rank "
                        "neighbours",
                        i, i + 1, lvl));
                }

                level.pi_rooted[i] = levels[lvl - 1].pi_rooted[left.right_pre];
            }
        }
    }
    /**
     * @brief Index of the parent gap of a chained gap.
     *
     * @param lvl Level of the gap.
     * @param i Index of the gap.
     * @return Index of the gap between the predecessors on level `lvl - 1`.
     */
    [[nodiscard]] std::size_t parent_of(const uint32_t lvl, const std::size_t i) const
    {
        return levels[lvl].nodes[i].right_pre;
    }
    /**
     * @brief Role of the node at a rank position.
     *
     * @param lvl Level.
     * @param r Rank position.
     * @return Role.
     */
    [[nodiscard]] node_role role_of(const uint32_t lvl, const std::size_t r) const
    {
        return levels[lvl].nodes[r].role;
    }
    /**
     * @brief Inner gap of the gate at a rank position: the gap between its two fanins on the level above.
     *
     * @param lvl Level of the gate.
     * @param r Rank position of the gate.
     * @return Inner gap.
     */
    [[nodiscard]] uint64_t inner_gap(const uint32_t lvl, const std::size_t r) const
    {
        // a negative gap of an unsettled round counts as zero here; its chain is raised at the end of the round
        return static_cast<uint64_t>(std::max<int64_t>(levels[lvl - 1].gaps[levels[lvl].nodes[r].left_pre], 0));
    }
    /**
     * @brief South offset that a node with a fixed role gives the free run next to it, if it fixes one.
     *
     * A fanout node's east consumer sits east of the wire end, its south consumer south of it, one tile further per
     * unit of the fanout root between them. A gate fixes nothing: its left gap grows with any offset of the run left
     * of it, its right gap keeps any offset of the run right of it.
     *
     * @param lvl Level.
     * @param r Rank position of the node with the fixed role.
     * @return South offset the neighbouring run should share, or `std::nullopt` for a gate.
     */
    [[nodiscard]] std::optional<uint64_t> fixed_dy(const uint32_t lvl, const std::size_t r) const
    {
        switch (role_of(lvl, r))
        {
            case node_role::GATE: return std::nullopt;
            case node_role::EAST_CONSUMER: return 0;
            case node_role::SOUTH_CONSUMER: return 1 + levels[lvl].fanout_roots[r - 1];
            default: return 0;
        }
    }
    /**
     * @brief Assigns the wiring of every node of a level for the given shift.
     *
     * Every wire runs east. A fanout node's east consumer sits east of the wire end (south offset 0), its south
     * consumer south of it (offset one plus the fanout root). A gate sits at the corner below its north fanin's wire
     * end and right of its west fanin, the north fanin's wire runs east by the level's shift minus the inner gap minus
     * one. A run of free single-fanin nodes between two fixed nodes keeps the offset of its left neighbour where the
     * right neighbour's offset is not smaller, and otherwise drops to the right neighbour's offset once, at the first
     * gap of the run whose chain ends at a PI root, else at its first gap; the drop shrinks that gap. A run at the
     * start or the end of the level takes the offset of its one fixed neighbour; a level without fixed nodes sits
     * east of its wires.
     *
     * @param lvl Level, at least 1.
     */
    void assign_wiring(const uint32_t lvl)
    {
        auto&      level = levels[lvl];
        const auto width = level.wiring.size();
        const auto s     = level.shift;

        const auto fixed = [&level](const std::size_t r) { return level.nodes[r].role != node_role::FREE; };

        std::vector<std::optional<uint64_t>> dy(width);  // south offset of every single-fanin node once decided

        for (std::size_t r = 0; r < width; ++r)
        {
            if (role_of(lvl, r) == node_role::GATE)
            {
                const auto inner = inner_gap(lvl, r);

                auto& w    = level.wiring[r];
                w.east     = s - 1 - inner;
                w.south    = 0;
                w.dy_left  = inner + 1;
                w.dy_right = 0;

                continue;
            }

            if (fixed(r))
            {
                dy[r] = fixed_dy(lvl, r);
            }
        }

        std::size_t r = 0;
        while (r < width)
        {
            if (fixed(r))
            {
                ++r;
                continue;
            }

            auto end = r;
            while (end < width && !fixed(end))
            {
                ++end;
            }
            // the run is [r, end)

            const auto left  = r > 0 ? fixed_dy(lvl, r - 1) : std::nullopt;
            const auto right = end < width ? fixed_dy(lvl, end) : std::nullopt;

            if (!left.has_value() || !right.has_value() || *left <= *right)
            {
                const auto value = left.value_or(right.value_or(0));
                std::fill(dy.begin() + static_cast<std::ptrdiff_t>(r), dy.begin() + static_cast<std::ptrdiff_t>(end),
                          value);
            }
            else
            {
                // the drop sits at one gap of the span r - 1 .. end - 1: the first PI-rooted one, else the first
                std::size_t drop = r - 1;
                for (auto g = r - 1; g < end; ++g)
                {
                    if (level.pi_rooted[g])
                    {
                        drop = g;
                        break;
                    }
                }

                for (auto i = r; i < end; ++i)
                {
                    dy[i] = i <= drop ? *left : *right;
                }
            }

            r = end;
        }

        for (std::size_t i = 0; i < width; ++i)
        {
            if (role_of(lvl, i) == node_role::GATE)
            {
                continue;
            }

            auto&      w     = level.wiring[i];
            const auto value = dy[i].value_or(0);

            w.south_of_wire = value > 0;
            w.south         = value > 0 ? value - 1 : 0;
            w.east          = s - 1 - w.south;
            w.dy_left       = value;
            w.dy_right      = value;
        }
    }
    /**
     * @brief Computes the shift of a level: one more than its largest inner gap and than its largest fanout root.
     *
     * @param lvl Level, at least 1.
     * @return Shift.
     */
    [[nodiscard]] uint64_t shift_of(const uint32_t lvl) const
    {
        uint64_t s = 1;

        for (std::size_t r = 0; r < levels[lvl].nodes.size(); ++r)
        {
            if (role_of(lvl, r) == node_role::GATE)
            {
                s = std::max(s, inner_gap(lvl, r) + 1);
            }
        }

        for (const auto d : levels[lvl].fanout_roots)
        {
            s = std::max(s, d + 1);
        }

        return s;
    }
    /**
     * @brief Root of a chain: its level and gap index.
     */
    struct root_ref
    {
        /**
         * @brief Level of the root gap.
         */
        uint32_t level;
        /**
         * @brief Index of the root gap on its level.
         */
        std::size_t index;
        /**
         * @brief Orders roots by level, then index.
         */
        [[nodiscard]] auto operator<=>(const root_ref&) const = default;
    };
    /**
     * @brief Finds the root of a chain.
     *
     * The walk follows parent gaps upwards. A forced root (the gap between the two consumers of a fanout node whose
     * south consumer is a gate) equals that gate's inner gap, so the walk continues at the inner gap. The walk ends at
     * a PI root or at a fanout root.
     *
     * @param lvl Level of a gap.
     * @param i Index of the gap.
     * @return The root.
     */
    [[nodiscard]] root_ref root_of(uint32_t lvl, std::size_t i) const
    {
        while (levels[lvl].kinds[i] != gap_kind::PI_ROOT && levels[lvl].kinds[i] != gap_kind::FANOUT_ROOT)
        {
            if (levels[lvl].kinds[i] == gap_kind::CHAINED)
            {
                i = parent_of(lvl, i);
            }
            else
            {
                i = levels[lvl].nodes[i + 1].left_pre;  // the south consumer is the gate
            }

            --lvl;
        }

        return {.level = lvl, .index = i};
    }
    /**
     * @brief Raises a root by a shortfall: a PI root directly, a fanout root by placing the south consumer further
     * south.
     *
     * @param root Root.
     * @param shortfall Amount to raise by.
     */
    void raise_root(const root_ref root, const uint64_t shortfall)
    {
        if (levels[root.level].kinds[root.index] == gap_kind::PI_ROOT)
        {
            pi_roots[root.index] += shortfall;
        }
        else
        {
            levels[root.level].fanout_roots[root.index] += shortfall;
        }
    }
    /**
     * @brief One propagation round: computes every gap of every level from the roots and raises the root of every
     * chain that holds a negative gap.
     *
     * The gaps below a negative gap are computed from it as they are, so the shortfall of the lowest negative gap of a
     * chain already contains the shortfalls above it; the root is raised once, by the largest shortfall on its chain.
     * Inner gaps of gates count as zero while negative.
     *
     * @return `true` iff no gap is negative.
     */
    [[nodiscard]] bool forward_pass()
    {
        std::ranges::transform(pi_roots, levels[0].gaps.begin(),
                               [](const uint64_t g) { return static_cast<int64_t>(g); });
        levels[0].shift = 0;

        std::map<root_ref, uint64_t> shortfalls{};

        for (uint32_t lvl = 1; lvl <= ntk.depth(); ++lvl)
        {
            auto& level = levels[lvl];
            level.shift = shift_of(lvl);
            assign_wiring(lvl);

            for (std::size_t i = 0; i < level.gaps.size(); ++i)
            {
                const auto& left  = level.wiring[i];
                const auto& right = level.wiring[i + 1];

                const int64_t parent =
                    level.kinds[i] == gap_kind::CHAINED ? levels[lvl - 1].gaps[parent_of(lvl, i)] : int64_t{-1};

                level.gaps[i] = parent + static_cast<int64_t>(right.dy_left) - static_cast<int64_t>(left.dy_right);

                if (level.gaps[i] < 0)
                {
                    auto& shortfall = shortfalls[root_of(lvl, i)];
                    shortfall       = std::max(shortfall, static_cast<uint64_t>(-level.gaps[i]));
                }
            }
        }

        for (const auto& [root, shortfall] : shortfalls)
        {
            raise_root(root, shortfall);
        }

        return shortfalls.empty();
    }
    /**
     * @brief Places every node of a level in rank order at the positions the settled gaps prescribe.
     *
     * @param lvl Level.
     * @throws std::logic_error If a node leaves the level's diagonal.
     */
    void place_level(const uint32_t lvl)
    {
        const auto& level = levels[lvl];

        std::optional<uint64_t> diagonal{};

        ntk.foreach_node_in_rank(lvl,
                                 [this, &level, &diagonal, lvl](const auto& n, const auto i)
                                 {
                                     if (ntk.is_constant(n))
                                     {
                                         return;
                                     }

                                     if (ntk.is_pi(n))
                                     {
                                         place_input(n, i);
                                     }
                                     else if (level.nodes[i].role != node_role::GATE)
                                     {
                                         place_single_fanin(n, level.wiring[i]);
                                     }
                                     else
                                     {
                                         place_two_fanin(n, level.wiring[i]);
                                     }

                                     if (lvl != 0)
                                     {
                                         check_diagonal(n, lvl, diagonal);
                                     }
                                 });
    }
    /**
     * @brief Places a primary input.
     *
     * The first input sits at the top right of the input diagonal, the others follow down the diagonal with the PI
     * roots as gaps. Inputs in the lower half of the diagonal are wired in from the left border, the others from the
     * top border. An input that drives nothing stays at the border without a wire.
     *
     * @param n Input node.
     * @param i Rank position of `n`.
     */
    void place_input(const node n, const std::size_t i)
    {
        if (i == 0)
        {
            const auto spread = std::accumulate(pi_roots.cbegin(), pi_roots.cend(), uint64_t{0});
            first_pos         = {static_cast<uint64_t>(ntk.num_pis()) - 1 + spread, 0};

            pi2node[n]  = place(layout, first_pos, placement_ntk, n);
            node2pos[n] = pi2node[n];
            place_t     = first_pos;

            return;
        }

        const auto step = static_cast<int32_t>(pi_roots[i - 1]) + 1;
        place_t         = {place_t.x - step, place_t.y + step};

        if (place_t.x == 0)
        {
            pi2node[n]  = place(layout, place_t, placement_ntk, n);
            node2pos[n] = pi2node[n];

            return;
        }

        const coordinate border = place_t.x < first_pos.x / 2 ? coordinate{0, place_t.y} : coordinate{place_t.x, 0};

        pi2node[n] = place(layout, border, placement_ntk, n);

        if (ntk.fanout_size(n) == 0)
        {
            node2pos[n] = pi2node[n];

            return;
        }

        node2pos[n] = layout.create_buf(wire_straight(layout, border, place_t), place_t);
    }
    /**
     * @brief Places a single-fanin node at the end of its predecessor's wire.
     *
     * @param n Node.
     * @param w Wiring of `n`.
     */
    void place_single_fanin(const node n, const node_wiring& w)
    {
        const auto pre   = ranked_fanins(ntk, n).front();
        const auto pre_t = wire_east_then_south(layout, layout.get_tile(node2pos[pre]), w.east, w.south);

        place_t = w.south_of_wire ? coordinate{pre_t.x, pre_t.y + 1} : coordinate{pre_t.x + 1, pre_t.y};

        node2pos[n] = connect_and_place_planar(layout, place_t, placement_ntk, n, pre_t);

        if (ntk.rank_position(n) == 0)
        {
            first_pos = place_t;
        }
    }
    /**
     * @brief Places a two-input gate at the corner below its north fanin's wire and right of its west fanin's wire.
     *
     * The north fanin's wire runs east by the level's shift minus the inner gap minus one, then straight south to the
     * gate. The west fanin's wire runs straight east to the gate.
     *
     * @param n Gate.
     * @param w Wiring of `n`.
     */
    void place_two_fanin(const node n, const node_wiring& w)
    {
        const auto fc     = networks::fanins(ntk, n);
        const auto fanins = ranked_fanins(ntk, n);

        const auto north_t = wire_east_then_south(layout, layout.get_tile(node2pos[fanins.front()]), w.east, 0);
        const auto west_t  = layout.get_tile(node2pos[fanins.back()]);

        place_t = {north_t.x, west_t.y};

        // the signals reach the gate in fanin order
        const bool first_is_north = fc.fanin_nodes.front() == fanins.front();

        node2pos[n] = connect_and_place_planar(layout, place_t, placement_ntk, n, first_is_north ? north_t : west_t,
                                               first_is_north ? west_t : north_t, fc.constant_fanin);

        if (ntk.rank_position(n) == 0)
        {
            first_pos = place_t;
        }
    }
    /**
     * @brief Checks that a placed node sits on the level's diagonal.
     *
     * @param n Node just placed.
     * @param lvl Level.
     * @param diagonal Diagonal of the level, set by its first node.
     * @throws std::logic_error If `n` is on another diagonal.
     */
    void check_diagonal(const node n, const uint32_t lvl, std::optional<uint64_t>& diagonal) const
    {
        const auto t   = layout.get_tile(node2pos[n]);
        const auto sum = static_cast<uint64_t>(t.x) + static_cast<uint64_t>(t.y);

        if (!diagonal.has_value())
        {
            diagonal = sum;
        }
        else if (sum != *diagonal)
        {
            throw std::logic_error(
                fmt::format("node {} of level {} sits on diagonal {} instead of {}", n, lvl, sum, *diagonal));
        }
    }

    /**
     * @brief The network.
     */
    const fanout_ntk ntk;
    /**
     * @brief Named technology network over the storage of `ntk`, the form that `place` reads gate functions from.
     */
    const tec_nt placement_ntk;
    /**
     * @brief Parameters.
     */
    const orthogonal_planar_params& ps;
    /**
     * @brief Statistics.
     */
    orthogonal_planar_stats& pst;
    /**
     * @brief Signal in the layout of every placed node.
     */
    mockturtle::node_map<typename Lyt::object_id, fanout_ntk> node2pos;
    /**
     * @brief Layout under construction.
     */
    Lyt layout;
    /**
     * @brief Primary input of the layout for every input node; sets the input order of the layout.
     */
    mockturtle::node_map<typename Lyt::object_id, fanout_ntk> pi2node{ntk};
    /**
     * @brief Gaps, chain structure, shift and wiring of every level.
     */
    std::vector<level_data> levels;
    /**
     * @brief Gap between every pair of neighbouring primary inputs.
     */
    std::vector<uint64_t> pi_roots;
    /**
     * @brief Tile of the first node of the last placed level, the top right end of the current diagonal.
     */
    coordinate first_pos{};
    /**
     * @brief Tile of the last placed node.
     */
    coordinate place_t{};
};

}  // namespace detail

/**
 * @brief Places and routes a planar, balanced, fanout-substituted, ranked logic network as a 2DDWave-clocked gate-level
 * layout without wire crossings, by propagating gaps between rank neighbours.
 *
 * This is the reference predecessor of `plane` and is not part of the library proper: it has no CLI command, no Python
 * binding and no documentation page. It keeps the diagonal model of PLANE, but every wire of a level runs east and a
 * node sits east or south of its wire end; only the gaps between rank neighbours vary. Where two nodes would come too
 * close, the algorithm widens the gap between the primary inputs the two nodes descend from, or between the two
 * consumers of the fanout node they descend from, and recomputes, until every gap is non-negative. A gate whose fanins
 * are apart pushes its level further east by that distance.
 *
 * @tparam Lyt Cartesian gate-level layout type.
 * @tparam Ntk Ranked technology network type.
 * @param ntk Network to place and route.
 * @param ps Parameters.
 * @param pst Statistics.
 * @return The layout.
 * @throws std::invalid_argument If the network has no primary inputs, a node drives more than two fanouts, a gate keeps
 * a single non-constant fanin next to a constant one, a fanin is not on the level above its node, or the rank order has
 * crossings.
 * @throws networks::high_degree_fanin_exception If a node has more than two non-constant fanins.
 * @throws std::runtime_error If the gaps diverge or do not settle within `max_rounds` rounds.
 * @throws std::logic_error If the placement contradicts the model; a defect, not an input error.
 */
template <typename Lyt, typename Ntk>
    requires std::derived_from<Lyt, layouts::cartesian_layout> &&
             std::same_as<typename Lyt::object_id, layouts::layout_object_id> &&
             std::derived_from<Ntk, networks::technology_network> && mockturtle::has_depth_v<Ntk> &&
             mockturtle::has_level_v<Ntk> && mockturtle::has_rank_position_v<Ntk> &&
             mockturtle::has_at_rank_position_v<Ntk> && mockturtle::has_foreach_node_in_rank_v<Ntk>
[[nodiscard]] Lyt orthogonal_planar(const Ntk& ntk, const orthogonal_planar_params& ps = {},
                                    orthogonal_planar_stats* pst = nullptr)
{
    detail::check_placement_preconditions(ntk);

    orthogonal_planar_stats st{};

    detail::orthogonal_planar_impl<Lyt, Ntk> p{ntk, ps, st};

    auto result = p.run();

    st.num_crossings = result.num_crossings();

    if (pst != nullptr)
    {
        *pst = st;
    }

    return result;
}

}  // namespace fiction::physical_design
