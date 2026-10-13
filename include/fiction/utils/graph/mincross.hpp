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
 * @brief Edge crossing minimization modeled after Graphviz's `mincross`.
 * @author Benjamin Hien (hibenj)
 * @author Marcel Walter (marcelwa)
 */

#pragma once

#include "fiction/synthesis/network_balancing.hpp"

#include <mockturtle/traits.hpp>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <limits>
#include <utility>
#include <vector>

namespace fiction::utils::graph
{

/**
 * Parameters for the `mincross` crossing minimization algorithm.
 */
struct mincross_params
{
    /**
     * Whether the rank positions of primary inputs (PIs) should remain fixed during the minimization process.
     * If set to `true`, PIs will not be reordered. Connected components are then ordered together, since ordering them
     * one after the other would move the inputs.
     */
    bool fixed_pis = false;
    /**
     * If `false`, skips optimization and only reports the current number of crossings.
     */
    bool optimize = true;
    /**
     * Maximum number of iterations per optimization pass (heuristic from Graphviz).
     * Larger values allow more refinement but increase runtime.
     * Default (`24`) works well for small and medium-sized networks.
     */
    uint64_t max_iter = 24;
    /**
     * Minimum number of consecutive iterations without sufficient improvement
     * before quitting early (heuristic from Graphviz). Prevents wasting time when the number of crossings
     * no longer decreases.
     */
    uint64_t min_quit = 8;
    /**
     * Convergence threshold: relative improvement factor required to reset the early-quit counter (heuristic from
     * Graphviz). If the current crossing count drops below (`convergence` * `best_cross`), where `best_cross` is the
     * best/lowest crossing count found so far, the process continues. Default is `0.995` (i.e., at least 0.5%
     * improvement required).
     */
    double convergence = 0.995;
    /**
     * Maximum number of iterations in the initial (pass 0) and refinement (pass 1)
     * phases of the crossing minimization procedure (heuristic from Graphviz).
     *
     * - In these early passes, the algorithm explores simple reorderings to quickly
     *   reduce crossings without investing in the full optimization effort.
     * - By default, the number of iterations is capped at `4` to prevent excessive
     *   runtime during initialization and refinement. This cap can be lifted if a
     *   larger global maximum (`ps.max_iter`) is set.
     * - In the full optimization pass (pass 2), `ps.max_iter` is always used instead.
     */
    uint64_t init_refine_max_iters = 4;
};

/**
 * Statistics collected during the execution of the `mincross` algorithm.
 */
struct mincross_stats
{
    /**
     * The total number of edge crossings after optimization.
     */
    uint64_t num_crossings = std::numeric_limits<uint64_t>::max();
};

/**
 * Implements the crossing minimization algorithm inspired by Graphviz's `mincross`.
 * This algorithm reorders nodes in ranks to reduce edge crossings in a leveled graph representation of the logic
 * network.
 *
 * The network is split into its connected components, each component is ordered on its own and the components are
 * placed one after the other, as in Graphviz. Within a component, the order is built twice by breadth-first search
 * (from the sources following fanouts, from the sinks following fanins), each seed is refined by a few iterations of
 * the weighted-median and transposition heuristics, and the better of the two is refined further. Unlike Graphviz, the
 * order the network comes with is kept as a candidate, so the result is never worse than the input.
 *
 * All work happens on plain arrays indexed by node; the ranked network is only read at the start and written at the
 * end.
 *
 * @tparam Ntk Logic network type that models a leveled circuit with rank information.
 */
template <typename Ntk>
class mincross_impl
{
  public:
    using node = typename Ntk::node;

    /**
     * Constructs the crossing minimization implementation object.
     *
     * @param src The logic network on which to perform crossing minimization.
     * @param p Configuration parameters for the algorithm (e.g., `optimize`)
     * @param st Statistics object to store the resulting number of crossings.
     */
    mincross_impl(Ntk src, const mincross_params& p, mincross_stats& st) : ntk(std::move(src)), ps(p), pst{st} {}

    /**
     * Runs the crossing minimization algorithm and returns a reordered network.
     *
     * @return A network with reordered nodes in ranks to reduce edge crossings.
     */
    [[nodiscard]] Ntk run()
    {
        build_graph();

        if (ps.optimize)
        {
            std::vector<std::vector<node>> result(max_rank + 1);

            for (const auto& component : components())
            {
                load(component);
                minimize_crossings(component);

                for (uint32_t r = 0; r <= max_rank; ++r)
                {
                    result[r].insert(result[r].end(), cur[r].cbegin(), cur[r].cend());
                }
            }

            ntk.set_all_ranks(result);
        }

        // count the final order with global positions
        std::vector<node> all{};
        all.reserve(num_nodes);
        for (uint32_t r = 0; r <= max_rank; ++r)
        {
            ntk.foreach_node_in_rank(r, [&all](const auto& n) { all.push_back(n); });
        }
        load(all);
        pst.num_crossings = ncross();

        return ntk;
    }

  private:
    /**
     * Scale of the median values, as in Graphviz (`MC_SCALE`): medians are integers, positions are multiplied by this.
     */
    static constexpr int64_t MC_SCALE = 256;
    /**
     * Median value of a node without neighbours in the adjacent rank.
     */
    static constexpr int64_t NO_MEDIAN = -1;

    /**
     * Reads the levels and the non-constant edges of the network into arrays.
     */
    void build_graph()
    {
        const auto size = ntk.size();

        fanins.assign(size, {});
        fanouts.assign(size, {});
        level_of.assign(size, 0);
        pos.assign(size, 0);
        mval.assign(size, NO_MEDIAN);
        mark.assign(size, 0);
        max_rank  = ntk.depth();
        num_nodes = 0;

        ntk.foreach_node(
            [this](const auto& n)
            {
                if (ntk.is_constant(n))
                {
                    return;
                }

                ++num_nodes;
                level_of[n] = ntk.level(n);

                ntk.foreach_fanin(n,
                                  [this, &n](const auto& f)
                                  {
                                      if (const auto src = ntk.get_node(f); !ntk.is_constant(src))
                                      {
                                          fanins[n].push_back(src);
                                          fanouts[src].push_back(n);
                                      }
                                  });
            });
    }

    /**
     * Splits the nodes into connected components, each in the order of the nodes in the network, the components in the
     * order of their first node. With fixed inputs there is one component holding every node.
     *
     * @return The components.
     */
    [[nodiscard]] std::vector<std::vector<node>> components()
    {
        std::vector<std::vector<node>> comps{};

        std::vector<node> nodes{};
        nodes.reserve(num_nodes);
        ntk.foreach_node(
            [this, &nodes](const auto& n)
            {
                if (!ntk.is_constant(n))
                {
                    nodes.push_back(n);
                }
            });

        if (ps.fixed_pis)
        {
            comps.push_back(std::move(nodes));
            return comps;
        }

        std::fill(mark.begin(), mark.end(), 0);

        std::vector<uint32_t> component_of(ntk.size(), 0);
        uint32_t              num_components = 0;
        std::deque<node>      queue{};

        for (const auto& n : nodes)
        {
            if (mark[n] != 0)
            {
                continue;
            }

            const auto c = num_components++;
            comps.emplace_back();

            mark[n] = 1;
            queue.push_back(n);

            while (!queue.empty())
            {
                const auto n0 = queue.front();
                queue.pop_front();
                component_of[n0] = c;

                for (const auto* adjacent : {&fanins[n0], &fanouts[n0]})
                {
                    for (const auto& m : *adjacent)
                    {
                        if (mark[m] == 0)
                        {
                            mark[m] = 1;
                            queue.push_back(m);
                        }
                    }
                }
            }
        }

        // nodes of a component in network order
        for (const auto& n : nodes)
        {
            comps[component_of[n]].push_back(n);
        }

        return comps;
    }

    /**
     * Loads the current rank order of the given nodes into the working order, positions counted within the component.
     *
     * @param nodes Nodes of one component.
     */
    void load(const std::vector<node>& nodes)
    {
        cur.assign(max_rank + 1, {});

        for (const auto& n : nodes)
        {
            cur[level_of[n]].push_back(n);
        }

        for (auto& rank : cur)
        {
            std::ranges::sort(rank, [this](const auto& a, const auto& b)
                              { return ntk.rank_position(a) < ntk.rank_position(b); });
            update_positions(rank);
        }
    }

    /**
     * Refreshes the positions of the nodes of a rank.
     *
     * @param rank Nodes of a rank.
     */
    void update_positions(const std::vector<node>& rank)
    {
        for (uint32_t i = 0; i < rank.size(); ++i)
        {
            pos[rank[i]] = i;
        }
    }

    /**
     * Saves the working order.
     */
    void save_best()
    {
        best = cur;
    }

    /**
     * Restores the saved order.
     */
    void restore_best()
    {
        cur = best;

        for (const auto& rank : cur)
        {
            update_positions(rank);
        }
    }

    /**
     * Performs the main iterative crossing minimization on one component (Graphviz `mincross`, start pass 0).
     *
     * @param nodes Nodes of the component in network order.
     */
    void minimize_crossings(const std::vector<node>& nodes)
    {
        // the order the component came with is the first candidate
        auto cur_cross  = ncross();
        auto best_cross = cur_cross;
        save_best();

        const auto fixed_inputs = cur[0];

        for (uint64_t pass = 0; pass <= 2; ++pass)
        {
            uint64_t max_this_pass = 0;

            if (pass <= 1)
            {
                max_this_pass = std::min(ps.init_refine_max_iters, ps.max_iter);

                build_ranks(nodes, pass, fixed_inputs);

                cur_cross = ncross();

                if (cur_cross <= best_cross)
                {
                    save_best();
                    best_cross = cur_cross;
                }
            }
            else
            {
                max_this_pass = ps.max_iter;

                if (cur_cross > best_cross)
                {
                    restore_best();
                }
                cur_cross = best_cross;
            }

            uint64_t trying = 0;

            for (uint64_t iter = 0; iter < max_this_pass; ++iter)
            {
                if (trying++ >= ps.min_quit || cur_cross == 0)
                {
                    break;
                }

                mincross_step(iter);

                cur_cross = ncross();

                if (cur_cross <= best_cross)
                {
                    save_best();
                    // Note: Conversion to double is safe here since best_cross < 2^53 in all practical cases.
                    if (static_cast<double>(cur_cross) < ps.convergence * static_cast<double>(best_cross))
                    {
                        trying = 0;
                    }
                    best_cross = cur_cross;
                }
            }

            if (cur_cross == 0)
            {
                break;
            }
        }

        if (cur_cross > best_cross)
        {
            restore_best();
        }

        if (best_cross > 0)
        {
            transpose(false);
        }
    }

    /**
     * Builds a fresh order by breadth-first search (Graphviz `build_ranks`): in pass 0 from the sources following
     * fanouts, in pass 1 from the sinks following fanins, the start nodes in network order. Fixed inputs are put back
     * into their order afterwards. The order is then improved by one transposition pass if it has crossings.
     *
     * @param nodes Nodes of the component in network order.
     * @param pass 0 or 1.
     * @param fixed_inputs Order of the input rank to restore if the inputs are fixed.
     */
    void build_ranks(const std::vector<node>& nodes, const uint64_t pass, const std::vector<node>& fixed_inputs)
    {
        for (auto& rank : cur)
        {
            rank.clear();
        }

        std::fill(mark.begin(), mark.end(), 0);

        std::deque<node> queue{};

        for (const auto& n : nodes)
        {
            const auto& other_edges = pass == 0 ? fanins[n] : fanouts[n];

            if (!other_edges.empty() || mark[n] != 0)
            {
                continue;
            }

            mark[n] = 1;
            queue.push_back(n);

            while (!queue.empty())
            {
                const auto n0 = queue.front();
                queue.pop_front();

                pos[n0] = static_cast<uint32_t>(cur[level_of[n0]].size());
                cur[level_of[n0]].push_back(n0);

                for (const auto& m : pass == 0 ? fanouts[n0] : fanins[n0])
                {
                    if (mark[m] == 0)
                    {
                        mark[m] = 1;
                        queue.push_back(m);
                    }
                }
            }
        }

        if (ps.fixed_pis)
        {
            cur[0] = fixed_inputs;
            update_positions(cur[0]);
        }

        if (ncross() > 0)
        {
            transpose(false);
        }
    }

    /**
     * Executes one full down or up pass of median ordering followed by transposition (Graphviz `mincross_step`).
     *
     * @param pass The current iteration number; it determines the direction and the tie handling.
     */
    void mincross_step(const uint64_t pass)
    {
        // Graphviz breaks ties in the reverse direction on two of every four iterations
        const bool reverse = pass % 4 < 2;

        if (pass % 2 == 0)
        {
            // down pass: from rank 1 to max_rank, each sorted by its fanins
            for (uint32_t r = 1; r <= max_rank; ++r)
            {
                medians(r, r - 1);
                reorder(r, reverse);
            }
        }
        else
        {
            // up pass: from max_rank - 1 down to 0, each sorted by its fanouts
            for (uint32_t r = max_rank; r-- > 0;)
            {
                if (r == 0 && ps.fixed_pis)
                {
                    continue;
                }

                medians(r, r + 1);
                reorder(r, reverse);
            }
        }

        transpose(!reverse);
    }

    /**
     * Computes the median values for the nodes in rank `r0` from the positions of their neighbours in rank `r1`
     * (Graphviz `medians`): the weighted median for an even number of neighbours, `NO_MEDIAN` for none.
     *
     * @param r0 Current rank.
     * @param r1 Adjacent rank to which connections are considered.
     */
    void medians(const uint32_t r0, const uint32_t r1)
    {
        std::vector<int64_t> list{};

        for (const auto& n : cur[r0])
        {
            list.clear();

            for (const auto& m : r1 > r0 ? fanouts[n] : fanins[n])
            {
                list.push_back(MC_SCALE * static_cast<int64_t>(pos[m]));
            }

            const auto j = list.size();

            switch (j)
            {
                case 0:
                {
                    mval[n] = NO_MEDIAN;
                    break;
                }
                case 1:
                {
                    mval[n] = list[0];
                    break;
                }
                case 2:
                {
                    mval[n] = (list[0] + list[1]) / 2;
                    break;
                }
                default:
                {
                    std::ranges::sort(list);

                    if (j % 2 == 1)
                    {
                        mval[n] = list[j / 2];
                    }
                    else
                    {
                        // weighted median
                        const auto rm    = j / 2;
                        const auto lm    = rm - 1;
                        const auto rspan = list[j - 1] - list[rm];
                        const auto lspan = list[lm] - list[0];

                        if (lspan == rspan)
                        {
                            mval[n] = (list[lm] + list[rm]) / 2;
                        }
                        else
                        {
                            const double w = static_cast<double>(list[lm]) * static_cast<double>(rspan) +
                                             static_cast<double>(list[rm]) * static_cast<double>(lspan);
                            mval[n]        = static_cast<int64_t>(w / static_cast<double>(lspan + rspan));
                        }
                    }
                    break;
                }
            }
        }
    }

    /**
     * Exchanges two nodes of the same rank.
     *
     * @param r Rank.
     * @param i Position of the first node.
     * @param k Position of the second node.
     */
    void exchange(const uint32_t r, const uint32_t i, const uint32_t k)
    {
        std::swap(cur[r][i], cur[r][k]);
        pos[cur[r][i]] = i;
        pos[cur[r][k]] = k;
    }

    /**
     * Reorders the nodes of a rank by their median values (Graphviz `reorder`): an exchange sweep over the nodes that
     * have a median, repeated as often as there are nodes. Nodes without a median stay where they are; equal medians
     * are exchanged when `reverse` is set.
     *
     * @param r The rank index.
     * @param reverse Whether to exchange nodes with equal medians.
     */
    void reorder(const uint32_t r, const bool reverse)
    {
        const auto& rank = cur[r];
        auto        ep   = static_cast<uint32_t>(rank.size());

        for (auto nelt = rank.size(); nelt-- > 0;)
        {
            uint32_t lp = 0;

            while (lp < ep)
            {
                // the leftmost node that can be compared
                while (lp < ep && mval[rank[lp]] < 0)
                {
                    ++lp;
                }
                if (lp >= ep)
                {
                    break;
                }

                // the next node that can be compared
                auto rp = lp + 1;
                while (rp < ep && mval[rank[rp]] < 0)
                {
                    ++rp;
                }
                if (rp >= ep)
                {
                    break;
                }

                const auto p1 = mval[rank[lp]];
                const auto p2 = mval[rank[rp]];

                if (p1 > p2 || (p1 == p2 && reverse))
                {
                    exchange(r, lp, rp);
                }

                lp = rp;
            }

            if (!reverse)
            {
                --ep;
            }
        }
    }

    /**
     * Performs pairwise transpositions within ranks until no swap reduces the crossings any further (Graphviz
     * `transpose`). A swap in a rank makes both neighbouring ranks candidates again.
     *
     * @param reverse Whether to swap neighbours whose crossings are equal.
     */
    void transpose(const bool reverse)
    {
        std::vector<uint8_t> candidate(max_rank + 1, 1);

        // every round that swaps removes at least one crossing, so the loop ends
        while (true)
        {
            uint64_t delta = 0;

            for (uint32_t r = 0; r <= max_rank; ++r)
            {
                if (r == 0 && ps.fixed_pis)
                {
                    continue;
                }

                if (candidate[r] != 0)
                {
                    delta += transpose_step(r, reverse, candidate);
                }
            }

            if (delta == 0)
            {
                break;
            }
        }
    }

    /**
     * Performs a single transposition pass for rank `r` (Graphviz `transpose_step`).
     *
     * @param r Rank index.
     * @param reverse Whether to swap neighbours whose crossings are equal.
     * @param candidate Ranks still worth a pass; updated for `r` and its neighbours.
     * @return The number of crossings removed.
     */
    uint64_t transpose_step(const uint32_t r, const bool reverse, std::vector<uint8_t>& candidate)
    {
        const auto& rank = cur[r];

        candidate[r] = 0;

        uint64_t rv = 0;

        for (uint32_t i = 0; i + 1 < rank.size(); ++i)
        {
            const auto v = rank[i];
            const auto w = rank[i + 1];

            uint64_t c0 = 0;
            uint64_t c1 = 0;

            if (r > 0)
            {
                c0 += in_cross(v, w);
                c1 += in_cross(w, v);
            }
            if (r < max_rank && !cur[r + 1].empty())
            {
                c0 += out_cross(v, w);
                c1 += out_cross(w, v);
            }

            if (c1 < c0 || (c0 > 0 && reverse && c1 == c0))
            {
                exchange(r, i, i + 1);
                rv += c0 - c1;

                candidate[r] = 1;
                if (r > 0)
                {
                    candidate[r - 1] = 1;
                }
                if (r < max_rank)
                {
                    candidate[r + 1] = 1;
                }
            }
        }

        return rv;
    }

    /**
     * Counts the crossings between the fanin edges of two nodes of one rank, `v` placed left of `w`.
     *
     * @param v Left node.
     * @param w Right node.
     * @return Number of crossings.
     */
    [[nodiscard]] uint64_t in_cross(const node& v, const node& w) const
    {
        uint64_t cross = 0;

        for (const auto& mw : fanins[w])
        {
            for (const auto& mv : fanins[v])
            {
                if (pos[mv] > pos[mw])
                {
                    ++cross;
                }
            }
        }

        return cross;
    }

    /**
     * Counts the crossings between the fanout edges of two nodes of one rank, `v` placed left of `w`.
     *
     * @param v Left node.
     * @param w Right node.
     * @return Number of crossings.
     */
    [[nodiscard]] uint64_t out_cross(const node& v, const node& w) const
    {
        uint64_t cross = 0;

        for (const auto& mw : fanouts[w])
        {
            for (const auto& mv : fanouts[v])
            {
                if (pos[mv] > pos[mw])
                {
                    ++cross;
                }
            }
        }

        return cross;
    }

    /**
     * Counts the crossings between rank `r` and rank `r + 1` with a count array (Graphviz `rcross`).
     *
     * @param r Lower rank.
     * @return Number of crossings between the two ranks.
     */
    [[nodiscard]] uint64_t rcross(const uint32_t r) const
    {
        std::vector<uint64_t> count(cur[r + 1].size() + 1, 0);

        uint64_t cross = 0;
        uint32_t max   = 0;

        for (const auto& top : cur[r])
        {
            if (max > 0)
            {
                for (const auto& head : fanouts[top])
                {
                    for (auto k = pos[head] + 1; k <= max; ++k)
                    {
                        cross += count[k];
                    }
                }
            }

            for (const auto& head : fanouts[top])
            {
                max = std::max(max, pos[head]);
                ++count[pos[head]];
            }
        }

        return cross;
    }

    /**
     * Counts the crossings of the working order.
     *
     * @return Number of crossings.
     */
    [[nodiscard]] uint64_t ncross() const
    {
        uint64_t total = 0;

        for (uint32_t r = 0; r < max_rank; ++r)
        {
            total += rcross(r);
        }

        return total;
    }

    /**
     * Logic network being reordered.
     */
    Ntk ntk;
    /**
     * Parameters for crossing minimization.
     */
    const mincross_params& ps;
    /**
     * Statistics that store the final number of crossings after optimization.
     */
    mincross_stats& pst;
    /**
     * Non-constant fanins of every node, indexed by node.
     */
    std::vector<std::vector<node>> fanins{};
    /**
     * Non-constant fanouts of every node, indexed by node.
     */
    std::vector<std::vector<node>> fanouts{};
    /**
     * Level of every node, indexed by node.
     */
    std::vector<uint32_t> level_of{};
    /**
     * Position of every node within its rank of the working order, indexed by node.
     */
    std::vector<uint32_t> pos{};
    /**
     * Median value of every node, indexed by node.
     */
    std::vector<int64_t> mval{};
    /**
     * Visited flags for the searches, indexed by node.
     */
    std::vector<uint8_t> mark{};
    /**
     * Working order: the nodes of the current component per rank.
     */
    std::vector<std::vector<node>> cur{};
    /**
     * Best order of the current component found so far.
     */
    std::vector<std::vector<node>> best{};
    /**
     * Highest rank.
     */
    uint32_t max_rank = 0;
    /**
     * Number of non-constant nodes.
     */
    uint32_t num_nodes = 0;
};

/**
 * Reimplementation of Graphviz's `mincross` algorithm for edge crossing minimization. This function reorders nodes in a
 * leveled logic network to minimize the number of edge crossings using iterative median and transpose heuristics.
 *
 * Reference implementation:
 * https://gitlab.com/graphviz/graphviz/-/blob/main/lib/dotgen/mincross.c
 *
 * For more on Graphviz's `dot` layout generation:
 * https://graphviz.org/docs/layouts/dot/
 *
 * @tparam Ntk A logic network type with level and rank support.
 * @param ntk The input leveled network whose ranks are to be reordered.
 * @param ps Configuration parameters for the minimization algorithm.
 * @param pst Optional pointer to a statistics structure for storing the resulting number of crossings.
 * @return A copy of the input network with reordered ranks to reduce edge crossings.
 */
template <typename Ntk>
Ntk mincross(Ntk& ntk, const mincross_params& ps = {}, mincross_stats* pst = nullptr)
{
    static_assert(mockturtle::is_network_type_v<Ntk>, "Ntk is not a network type");
    static_assert(mockturtle::has_rank_position_v<Ntk>, "Ntk does not implement the rank_position function");
    static_assert(mockturtle::has_foreach_node_in_rank_v<Ntk>,
                  "Ntk does not implement the foreach_node_in_rank function");
    static_assert(mockturtle::has_level_v<Ntk>, "Ntk does not implement the level function");
    static_assert(mockturtle::has_depth_v<Ntk>, "Ntk does not implement the depth function");

    assert(fiction::synthesis::is_balanced(ntk) &&
           "Networks have to be balanced for level-by-level crossing minimization");

    mincross_stats st{};
    mincross_impl  p{ntk, ps, st};

    auto result = p.run();

    if (pst)
    {
        *pst = st;
    }

    return result;
}

}  // namespace fiction::utils::graph
