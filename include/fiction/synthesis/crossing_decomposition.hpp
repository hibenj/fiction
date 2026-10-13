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
 * @brief Decomposes LUT pairs with shared fanins into a shared node when that lowers the crossing count of a ranked
 * network.
 * @author Benjamin Hien (hibenj)
 */

#pragma once

#include "fiction/networks/views/mutable_rank_view.hpp"
#include "fiction/synthesis/network_balancing.hpp"
#include "fiction/utils/graph/mincross.hpp"

#include <fmt/format.h>
#include <kitty/bit_operations.hpp>
#include <kitty/constructors.hpp>
#include <kitty/dynamic_truth_table.hpp>
#include <kitty/operations.hpp>
#include <mockturtle/traits.hpp>
#include <mockturtle/utils/node_map.hpp>
#include <mockturtle/utils/stopwatch.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace fiction::synthesis
{

/**
 * Parameters for the crossing-aware LUT decomposition.
 */
struct crossing_decomposition_params
{
    /**
     * Passed to `mincross` for the initial ordering and for every crossing count.
     */
    fiction::utils::graph::mincross_params mincross{};
    /**
     * Smallest number of shared fanins a LUT pair must have to be considered for a shared node.
     */
    uint32_t min_shared_fanins = 2;
    /**
     * Number of levels below the level of a decomposed pair that may be re-sorted locally before the candidate is
     * counted: the pair's level and that many levels below it are swept by adjacent swaps while they lower the
     * crossing count. The level of the shared node is the first one below the pair, the level of its sources the
     * second. Every other level keeps its order; the pass never reorders the whole network again. `0` disables the
     * re-sorting.
     */
    uint32_t local_transpose_levels = 3;
};

/**
 * Statistics of the crossing-aware LUT decomposition.
 */
struct crossing_decomposition_stats
{
    /**
     * Crossings of the ranked network after the initial ordering, before any decomposition.
     */
    uint64_t crossings_before = 0;
    /**
     * Crossings of the returned network.
     */
    uint64_t crossings_after = 0;
    /**
     * Number of accepted decompositions, i.e., of shared nodes that were created.
     */
    uint32_t decompositions = 0;
    /**
     * Number of levels inserted to hold shared nodes.
     */
    uint32_t levels_inserted = 0;
    /**
     * Number of adjacent swaps the local re-sorting made in accepted candidates.
     */
    uint64_t swaps = 0;
    /**
     * Total runtime, the initial ordering included.
     */
    mockturtle::stopwatch<>::duration time_total{0};
    /**
     * Writes the statistics to a stream.
     *
     * @param out Stream to write to.
     */
    void report(std::ostream& out = std::cout) const
    {
        out << fmt::format("[i] total time        = {:.2f} secs\n", mockturtle::to_seconds(time_total));
        out << fmt::format("[i] crossings before  = {}\n", crossings_before);
        out << fmt::format("[i] crossings after   = {}\n", crossings_after);
        out << fmt::format("[i] decompositions    = {}\n", decompositions);
        out << fmt::format("[i] levels inserted   = {}\n", levels_inserted);
        out << fmt::format("[i] local swaps       = {}\n", swaps);
    }
};

namespace detail
{

/**
 * Largest number of inputs of a shared node the decomposition search enumerates. The search tries every function over
 * the shared inputs, 2^(2^k) for k inputs, which is cheap up to k = 4 (65536 functions) and prohibitive beyond.
 */
inline constexpr uint32_t MAX_SHARED_NODE_INPUTS = 4u;
/**
 * Upper bound on the sweeps of the local re-sorting of one candidate. Every sweep that is not the last one removes at
 * least one crossing, so the bound only guards against pathological inputs.
 */
inline constexpr uint32_t MAX_LOCAL_TRANSPOSE_SWEEPS = 64u;

/**
 * A decomposition of two LUTs f1 and f2 into one shared node h and two new LUTs g1 and g2 such that
 * f1(z1, y) = g1(z1, h(y)) and f2(z2, y) = g2(z2, h(y)), where y are the fanins both LUTs share that feed h.
 *
 * @tparam Node Node type of the network.
 */
template <typename Node>
struct shared_decomposition
{
    /**
     * Fanins of the shared node, in its variable order: variable `i` of `h` is `h_fanins[i]`.
     */
    std::vector<Node> h_fanins{};
    /**
     * Function of the shared node over `h_fanins`.
     */
    kitty::dynamic_truth_table h{};
    /**
     * Fanins of the new LUT that replaces f1, without the shared node: variable `i` of `g1` is `g1_fanins[i]` for
     * `i < g1_fanins.size()`, and the last variable of `g1` is the shared node.
     */
    std::vector<Node> g1_fanins{};
    /**
     * Function of the new LUT that replaces f1 over `g1_fanins` plus the shared node as its last variable.
     */
    kitty::dynamic_truth_table g1{};
    /**
     * Fanins of the new LUT that replaces f2, without the shared node (see `g1_fanins`).
     */
    std::vector<Node> g2_fanins{};
    /**
     * Function of the new LUT that replaces f2 over `g2_fanins` plus the shared node as its last variable.
     */
    kitty::dynamic_truth_table g2{};
};

/**
 * Tries to write a LUT function f as g(z, h(y)), where y are the variables of f at the positions `ys` and z are the
 * remaining variables in their original order. For every assignment of z, all y with the same value of h(y) must give
 * the same value of f; then g(z, b) is that value.
 *
 * @param f Function of the LUT.
 * @param ys Positions of the variables of f that feed h, in the variable order of h.
 * @param h Candidate function of the shared node over the variables `ys`.
 * @return The outer function g over the z variables plus h as its last variable, or `std::nullopt` if f is not
 * decomposable with this h.
 */
[[nodiscard]] inline std::optional<kitty::dynamic_truth_table>
outer_function(const kitty::dynamic_truth_table& f, const std::vector<uint32_t>& ys,
               const kitty::dynamic_truth_table& h) noexcept
{
    const auto num_vars = f.num_vars();

    std::vector<uint32_t> zs{};
    zs.reserve(num_vars - ys.size());

    for (uint32_t v = 0; v < num_vars; ++v)
    {
        if (std::find(ys.cbegin(), ys.cend(), v) == ys.cend())
        {
            zs.push_back(v);
        }
    }

    kitty::dynamic_truth_table g{static_cast<uint32_t>(zs.size() + 1)};

    const auto num_z_assignments = uint64_t{1} << zs.size();
    const auto num_y_assignments = uint64_t{1} << ys.size();

    for (uint64_t z = 0; z < num_z_assignments; ++z)
    {
        // the value f takes on the class h(y) = b, once known; 2 = not seen yet
        std::array<uint8_t, 2> class_value{2u, 2u};

        for (uint64_t y = 0; y < num_y_assignments; ++y)
        {
            // assemble the minterm of f from the z and y bits
            uint64_t minterm{0};

            for (std::size_t i = 0; i < zs.size(); ++i)
            {
                minterm |= ((z >> i) & 1u) << zs[i];
            }
            for (std::size_t i = 0; i < ys.size(); ++i)
            {
                minterm |= ((y >> i) & 1u) << ys[i];
            }

            const auto b = kitty::get_bit(h, y);
            const auto v = static_cast<uint8_t>(kitty::get_bit(f, minterm));

            if (class_value[b] == 2u)
            {
                class_value[b] = v;
            }
            else if (class_value[b] != v)
            {
                return std::nullopt;
            }
        }

        for (std::size_t b = 0; b < 2; ++b)
        {
            // every class is non-empty since h is not constant
            if (class_value[b] == 1u)
            {
                kitty::set_bit(g, z | (static_cast<uint64_t>(b) << zs.size()));
            }
        }
    }

    return g;
}

/**
 * Searches for a node that two LUTs can share. Given the LUTs' functions and fanin lists (variable `i` of a function
 * is fanin `i`), the shared fanins S are the fanins both lists contain. For every subset Y of S with at least two and
 * at most `MAX_SHARED_NODE_INPUTS` fanins, larger subsets first, and for every function h over Y that depends on all of
 * Y, both LUTs are tested for the form f(z, h(y)) (see `outer_function`). The first decomposition found is returned.
 *
 * A larger Y removes more crossings: two LUTs in one rank that share k fanins carry k(k-1)/2 crossings among their
 * fanin edges in any order, and the shared node collapses the k edges to each LUT into one.
 *
 * @tparam Node Node type of the network.
 * @tparam Pred Predicate type on nodes.
 * @param f1 Function of the first LUT.
 * @param fanins1 Fanins of the first LUT in variable order.
 * @param f2 Function of the second LUT.
 * @param fanins2 Fanins of the second LUT in variable order.
 * @param shareable Tells for a fanin whether it may feed the shared node; fanins it rejects do not count as shared.
 * @return The decomposition with the largest shared node found, or `std::nullopt` if no subset of the shared fanins
 * admits one.
 */
template <typename Node, typename Pred>
[[nodiscard]] std::optional<shared_decomposition<Node>>
find_shared_decomposition(const kitty::dynamic_truth_table& f1, const std::vector<Node>& fanins1,
                          const kitty::dynamic_truth_table& f2, const std::vector<Node>& fanins2, Pred&& shareable)
{
    assert(f1.num_vars() == fanins1.size() && "f1 must have one variable per fanin");
    assert(f2.num_vars() == fanins2.size() && "f2 must have one variable per fanin");

    // shared fanins in the order of the first LUT, with their positions in both LUTs
    struct shared_fanin
    {
        Node     n;
        uint32_t pos1;
        uint32_t pos2;
    };

    std::vector<shared_fanin> shared{};

    for (uint32_t i = 0; i < fanins1.size(); ++i)
    {
        if (!shareable(fanins1[i]))
        {
            continue;
        }

        if (const auto it = std::find(fanins2.cbegin(), fanins2.cend(), fanins1[i]); it != fanins2.cend())
        {
            shared.push_back({fanins1[i], i, static_cast<uint32_t>(std::distance(fanins2.cbegin(), it))});
        }
    }

    if (shared.size() < 2)
    {
        return std::nullopt;
    }

    const auto largest = std::min(static_cast<uint32_t>(shared.size()), MAX_SHARED_NODE_INPUTS);

    for (auto k = largest; k >= 2; --k)
    {
        // every subset Y of the shared fanins with k members, encoded as a bit mask over `shared`
        for (uint64_t mask = 0; mask < (uint64_t{1} << shared.size()); ++mask)
        {
            if (static_cast<uint32_t>(std::popcount(mask)) != k)
            {
                continue;
            }

            std::vector<Node>     y_nodes{};
            std::vector<uint32_t> ys1{};
            std::vector<uint32_t> ys2{};

            for (std::size_t i = 0; i < shared.size(); ++i)
            {
                if ((mask >> i) & 1u)
                {
                    y_nodes.push_back(shared[i].n);
                    ys1.push_back(shared[i].pos1);
                    ys2.push_back(shared[i].pos2);
                }
            }

            kitty::dynamic_truth_table h{k};

            const auto num_functions = uint64_t{1} << (uint64_t{1} << k);

            for (uint64_t bits = 0; bits < num_functions; ++bits)
            {
                kitty::create_from_words(h, &bits, &bits + 1);

                // a shared node must use every one of its inputs, otherwise it removes no edge for that input;
                // this also excludes the constants and the (complemented) projections
                bool depends_on_all = true;

                for (uint32_t v = 0; v < k; ++v)
                {
                    if (!kitty::has_var(h, static_cast<uint8_t>(v)))
                    {
                        depends_on_all = false;
                        break;
                    }
                }

                if (!depends_on_all)
                {
                    continue;
                }

                const auto g1 = outer_function(f1, ys1, h);

                if (!g1.has_value())
                {
                    continue;
                }

                const auto g2 = outer_function(f2, ys2, h);

                if (!g2.has_value())
                {
                    continue;
                }

                shared_decomposition<Node> result{};
                result.h_fanins = y_nodes;
                result.h        = h;
                result.g1       = *g1;
                result.g2       = *g2;

                for (uint32_t i = 0; i < fanins1.size(); ++i)
                {
                    if (std::find(ys1.cbegin(), ys1.cend(), i) == ys1.cend())
                    {
                        result.g1_fanins.push_back(fanins1[i]);
                    }
                }
                for (uint32_t i = 0; i < fanins2.size(); ++i)
                {
                    if (std::find(ys2.cbegin(), ys2.cend(), i) == ys2.cend())
                    {
                        result.g2_fanins.push_back(fanins2[i]);
                    }
                }

                return result;
            }
        }
    }

    return std::nullopt;
}

/**
 * Searches for a node two LUTs can share, with every common fanin allowed to feed it.
 *
 * @tparam Node Node type of the network.
 * @param f1 Function of the first LUT.
 * @param fanins1 Fanins of the first LUT in variable order.
 * @param f2 Function of the second LUT.
 * @param fanins2 Fanins of the second LUT in variable order.
 * @return The decomposition with the largest shared node found, or `std::nullopt` if no subset of the shared fanins
 * admits one.
 */
template <typename Node>
[[nodiscard]] std::optional<shared_decomposition<Node>>
find_shared_decomposition(const kitty::dynamic_truth_table& f1, const std::vector<Node>& fanins1,
                          const kitty::dynamic_truth_table& f2, const std::vector<Node>& fanins2)
{
    return find_shared_decomposition(f1, fanins1, f2, fanins2, [](const Node&) { return true; });
}

/**
 * Median of a list of rank positions: the middle one, or the mean of the two middle ones for an even count.
 *
 * @param positions Positions; sorted by this function.
 * @return The median, or 0 for an empty list.
 */
[[nodiscard]] inline double median_position(std::vector<uint32_t>& positions) noexcept
{
    if (positions.empty())
    {
        return 0.0;
    }

    std::ranges::sort(positions);

    const auto mid = positions.size() / 2;

    if (positions.size() % 2 == 1)
    {
        return static_cast<double>(positions[mid]);
    }

    return (static_cast<double>(positions[mid - 1]) + static_cast<double>(positions[mid])) / 2.0;
}

/**
 * Implementation of the crossing-aware LUT decomposition.
 *
 * The pass walks the levels of the ordered network from the inputs to the outputs. In every level it looks for a pair
 * of LUTs that share fanins and admit a shared node (`find_shared_decomposition`). A candidate is materialized into a
 * fresh network whose rank order is derived from the current one, counted with `mincross` without reordering, and
 * accepted only if the count drops; the search then restarts in the same level on the new network. The first accepted
 * decomposition of a level inserts a new level below it that holds the shared node and one buffer for every other
 * node of the level below that still feeds the level; later decompositions of the same level put their shared node
 * into that inserted level. Existing ranks are never re-sorted.
 *
 * @tparam Ntk Balanced LUT network type with node functions.
 */
template <typename Ntk>
class crossing_decomposition_impl
{
  public:
    /**
     * Ranked network type the algorithm works on and returns.
     */
    using ranked_network = fiction::networks::views::mutable_rank_view<Ntk>;
    /**
     * Node type.
     */
    using node = mockturtle::node<Ntk>;
    /**
     * Signal type.
     */
    using signal = mockturtle::signal<Ntk>;

    /**
     * Creates the implementation.
     *
     * @param src Source network.
     * @param p Parameters.
     * @param st Statistics.
     */
    crossing_decomposition_impl(const Ntk& src, const crossing_decomposition_params& p,
                                crossing_decomposition_stats& st) :
            ntk{src},
            ps{p},
            pst{st}
    {}

    /**
     * Runs the algorithm: ranks the network, orders it with `mincross`, and then improves it level by level.
     *
     * @return The ordered network with the accepted decompositions applied.
     */
    [[nodiscard]] ranked_network run()
    {
        ranked_network ranked{ntk};

        fiction::utils::graph::mincross_stats mincross_st{};

        auto current           = fiction::utils::graph::mincross(ranked, ps.mincross, &mincross_st);
        auto current_crossings = mincross_st.num_crossings;

        pst.crossings_before = current_crossings;

        const auto original_depth = current.depth();

        // levels inserted so far; the original level `r_orig` now sits at `r_orig + shift`
        uint32_t shift = 0;

        for (uint32_t r_orig = 1; r_orig <= original_depth; ++r_orig)
        {
            auto r             = r_orig + shift;
            bool inserted_here = false;

            while (auto improved = find_improvement(current, r, inserted_here, current_crossings))
            {
                current           = std::move(improved->network);
                current_crossings = improved->crossings;
                pst.swaps += improved->swaps;
                ++pst.decompositions;

                if (!inserted_here)
                {
                    // the first accepted decomposition of this level inserted a level below it
                    inserted_here = true;
                    ++shift;
                    ++r;
                    ++pst.levels_inserted;
                }
            }
        }

        pst.crossings_after = current_crossings;

        return current;
    }

  private:
    /**
     * A materialized candidate with its crossing count.
     */
    struct improvement
    {
        ranked_network network;
        uint64_t       crossings;
        uint64_t       swaps;
    };

    /**
     * Looks for the first pair of LUTs in level `r` whose decomposition lowers the crossing count below `bound`.
     *
     * @param cur Current network.
     * @param r Level to search.
     * @param below_is_inserted Whether level `r - 1` was inserted by this pass for level `r`; then only its buffers may
     * feed a new shared node, and no further level is inserted.
     * @param bound Crossing count to beat.
     * @return The improved network and its count, or `std::nullopt` if no pair improves on the bound.
     */
    [[nodiscard]] std::optional<improvement> find_improvement(const ranked_network& cur, const uint32_t r,
                                                              const bool below_is_inserted, const uint64_t bound) const
    {
        // the LUTs of the level, left to right; buffers compute no function worth decomposing
        std::vector<node> luts{};
        cur.foreach_node_in_rank(r,
                                 [&cur, &luts](const auto& n)
                                 {
                                     if (!cur.is_constant(n) && !cur.is_ci(n) && !cur.is_buf(n))
                                     {
                                         luts.push_back(n);
                                     }
                                 });

        const auto shareable = [&cur, below_is_inserted](const node& n)
        { return !cur.is_constant(n) && (!below_is_inserted || cur.is_buf(n)); };

        std::vector<std::vector<node>> fanins(luts.size());

        for (std::size_t i = 0; i < luts.size(); ++i)
        {
            cur.foreach_fanin(luts[i], [&cur, &fanins, i](const auto& f) { fanins[i].push_back(cur.get_node(f)); });
        }

        for (std::size_t i = 0; i < luts.size(); ++i)
        {
            for (std::size_t j = i + 1; j < luts.size(); ++j)
            {
                const auto num_shared = static_cast<uint32_t>(std::count_if(
                    fanins[i].cbegin(), fanins[i].cend(),
                    [&fanins, &shareable, j](const auto& f)
                    {
                        return shareable(f) && std::find(fanins[j].cbegin(), fanins[j].cend(), f) != fanins[j].cend();
                    }));

                if (num_shared < std::max(ps.min_shared_fanins, 2u))
                {
                    continue;
                }

                const auto dec = find_shared_decomposition(lut_function(cur, luts[i], fanins[i]), fanins[i],
                                                           lut_function(cur, luts[j], fanins[j]), fanins[j], shareable);

                if (!dec.has_value())
                {
                    continue;
                }

                auto candidate = materialize(cur, r, luts[i], luts[j], *dec, !below_is_inserted);

                // the level of the pair in the candidate: one higher if a level was inserted below it
                const auto pair_level = below_is_inserted ? r : r + 1;

                const auto swaps = ps.local_transpose_levels > 0u ? transpose_locally(candidate, pair_level) : 0u;

                const auto crossings = count_crossings(candidate);

                if (crossings < bound)
                {
                    return improvement{std::move(candidate), crossings, swaps};
                }
            }
        }

        return std::nullopt;
    }

    /**
     * Returns the function of a LUT over exactly its fanins. A LUT mapper may store a function with more variables
     * than the node has fanins (the unused top variables are left in the table); mockturtle evaluates such a node with
     * those variables at zero, which is the low part of the table.
     *
     * @param cur Network.
     * @param n LUT.
     * @param fanins Its fanins.
     * @return The function over `fanins.size()` variables.
     */
    [[nodiscard]] static kitty::dynamic_truth_table lut_function(const ranked_network& cur, const node& n,
                                                                 const std::vector<node>& fanins)
    {
        auto f = cur.node_function(n);

        assert(f.num_vars() >= fanins.size() && "a LUT cannot have more fanins than variables");

        if (f.num_vars() > fanins.size())
        {
            f = kitty::shrink_to(f, static_cast<uint32_t>(fanins.size()));
        }

        return f;
    }

    /**
     * Re-sorts the level of a decomposed pair and the `local_transpose_levels` levels below it by adjacent swaps. A
     * sweep walks the window from the pair's level downwards and swaps two neighbours whenever that lowers the
     * crossings of their edges with both neighbouring levels, or keeps them equal: an equal swap moves a crossing one
     * level down, where the next level of the same sweep can remove it. A sweep that removes no crossing in total is
     * undone, so the result never contains idle moves. Levels outside the window keep their order; the inputs keep
     * theirs if the `mincross` parameters fix them.
     *
     * @param ranked Candidate to re-sort in place.
     * @param pair_level Level of the decomposed pair in `ranked`.
     * @return Number of swaps made.
     */
    [[nodiscard]] uint64_t transpose_locally(ranked_network& ranked, const uint32_t pair_level) const
    {
        const auto lowest  = pair_level > ps.local_transpose_levels ? pair_level - ps.local_transpose_levels : 0u;
        const auto highest = pair_level;

        // fanouts of the nodes in the window, collected from the fanins of the level above each one
        std::unordered_map<node, std::vector<node>> fanouts{};

        for (uint32_t l = lowest; l < std::min(highest + 1u, ranked.depth()); ++l)
        {
            ranked.foreach_node_in_rank(l + 1u,
                                        [&ranked, &fanouts](const auto& n)
                                        {
                                            ranked.foreach_fanin(n,
                                                                 [&ranked, &fanouts, &n](const auto& f)
                                                                 {
                                                                     if (const auto src = ranked.get_node(f);
                                                                         !ranked.is_constant(src))
                                                                     {
                                                                         fanouts[src].push_back(n);
                                                                     }
                                                                 });
                                        });
        }

        const auto positions_of = [&ranked](const std::vector<node>& nodes)
        {
            std::vector<uint32_t> positions{};
            positions.reserve(nodes.size());

            for (const auto& n : nodes)
            {
                positions.push_back(ranked.rank_position(n));
            }

            return positions;
        };

        const auto fanin_nodes = [&ranked](const node& n)
        {
            std::vector<node> nodes{};
            nodes.reserve(ranked.fanin_size(n));

            ranked.foreach_fanin(n,
                                 [&ranked, &nodes](const auto& f)
                                 {
                                     if (const auto src = ranked.get_node(f); !ranked.is_constant(src))
                                     {
                                         nodes.push_back(src);
                                     }
                                 });

            return nodes;
        };

        // crossings between two sets of edges that end at the given positions, the first set's nodes placed first
        const auto count = [](const std::vector<uint32_t>& a, const std::vector<uint32_t>& b)
        {
            uint64_t crossings = 0;

            for (const auto x : a)
            {
                for (const auto y : b)
                {
                    if (x > y)
                    {
                        ++crossings;
                    }
                }
            }

            return crossings;
        };

        // crossings between the edges of `left` and `right`, both in `level` with `left` placed first
        const auto pair_crossings = [&ranked, &fanouts, &positions_of, &fanin_nodes,
                                     &count](const node& left, const node& right, const uint32_t level)
        {
            uint64_t crossings = 0;

            if (level > 0u)
            {
                crossings += count(positions_of(fanin_nodes(left)), positions_of(fanin_nodes(right)));
            }
            if (level < ranked.depth())
            {
                crossings += count(positions_of(fanouts[left]), positions_of(fanouts[right]));
            }

            return crossings;
        };

        uint64_t swaps = 0;

        for (uint32_t sweep = 0; sweep < MAX_LOCAL_TRANSPOSE_SWEEPS; ++sweep)
        {
            // the window before the sweep, to undo a sweep that only moves crossings around
            std::vector<std::vector<node>> before{};
            before.reserve(highest - lowest + 1u);

            for (uint32_t l = lowest; l <= highest; ++l)
            {
                before.push_back(ranked.get_ranks(l));
            }

            uint64_t removed     = 0;
            uint64_t sweep_swaps = 0;

            for (uint32_t l = highest + 1u; l-- > lowest;)
            {
                if (l == 0u && ps.mincross.fixed_pis)
                {
                    continue;
                }

                const auto width = ranked.rank_width(l);

                for (uint32_t i = 0; i + 1u < width; ++i)
                {
                    const auto v = ranked.at_rank_position(l, i);
                    const auto w = ranked.at_rank_position(l, i + 1u);

                    const auto current = pair_crossings(v, w, l);
                    const auto swapped = pair_crossings(w, v, l);

                    if (swapped < current || (swapped == current && current > 0u))
                    {
                        ranked.swap(v, w);
                        removed += current - swapped;
                        ++sweep_swaps;
                    }
                }
            }

            if (removed == 0u)
            {
                for (uint32_t l = lowest; l <= highest; ++l)
                {
                    ranked.set_ranks(l, before[l - lowest]);
                }

                break;
            }

            swaps += sweep_swaps;
        }

        return swaps;
    }

    /**
     * Counts the crossings of a ranked network without reordering it, with the `mincross` parameters of the pass.
     *
     * @param ranked Ranked network.
     * @return Number of crossings.
     */
    [[nodiscard]] uint64_t count_crossings(const ranked_network& ranked) const
    {
        auto count_ps     = ps.mincross;
        count_ps.optimize = false;

        fiction::utils::graph::mincross_stats st{};
        fiction::utils::graph::mincross(ranked, count_ps, &st);

        return st.num_crossings;
    }

    /**
     * Builds the network in which the LUTs `n1` and `n2` of level `r` are replaced by `dec`: a fresh network is
     * created level by level in the rank order of `cur`, and the result is ranked with that order.
     *
     * With `insert_level`, a new level is placed between `r - 1` and `r`: it holds the shared node at the median
     * position of its fanins and one buffer for every node of level `r - 1` that still feeds level `r`, each at its
     * source's position. Without it, level `r - 1` is the level inserted earlier for level `r`: the shared node is
     * placed there over the sources of the buffers it replaces, and a buffer that fed only `n1` and `n2` is dropped.
     *
     * @param cur Current network.
     * @param r Level of the pair.
     * @param n1 First LUT of the pair.
     * @param n2 Second LUT of the pair.
     * @param dec Decomposition of the pair.
     * @param insert_level Whether to insert a level for the shared node.
     * @return The ranked network with the decomposition applied.
     */
    [[nodiscard]] ranked_network materialize(const ranked_network& cur, const uint32_t r, const node& n1,
                                             const node& n2, const shared_decomposition<node>& dec,
                                             const bool insert_level) const
    {
        Ntk dest{};

        mockturtle::node_map<signal, ranked_network> old2new{cur};

        old2new[cur.get_constant(false)] = dest.get_constant(false);
        if (cur.get_node(cur.get_constant(true)) != cur.get_node(cur.get_constant(false)))
        {
            old2new[cur.get_constant(true)] = dest.get_constant(true);
        }

        cur.foreach_pi_unranked([&dest, &old2new](const auto& pi) { old2new[pi] = dest.create_pi(); });

        std::vector<std::vector<node>> new_ranks{};
        new_ranks.reserve(cur.depth() + 2);

        // the nodes of an old rank in the new network, in the old order
        const auto map_rank = [&dest, &old2new](const std::vector<node>& old_rank)
        {
            std::vector<node> mapped{};
            mapped.reserve(old_rank.size());
            std::ranges::transform(old_rank, std::back_inserter(mapped),
                                   [&dest, &old2new](const auto& n) { return dest.get_node(old2new[n]); });
            return mapped;
        };

        // copies a node with its fanins mapped
        const auto clone = [&cur, &dest, &old2new](const node& n)
        {
            std::vector<signal> children{};
            children.reserve(cur.fanin_size(n));
            cur.foreach_fanin(n, [&cur, &old2new, &children](const auto& f)
                              { children.push_back(old2new[cur.get_node(f)]); });
            old2new[n] = dest.clone_node(cur, n, children);
        };

        const auto is_y = [&dec](const node& n)
        { return std::find(dec.h_fanins.cbegin(), dec.h_fanins.cend(), n) != dec.h_fanins.cend(); };

        // nodes of level r - 1 that keep a fanout into level r once the Y edges into n1 and n2 are gone
        std::unordered_set<node> keeps_fanout{};
        cur.foreach_node_in_rank(r,
                                 [&cur, &n1, &n2, &is_y, &keeps_fanout](const auto& m)
                                 {
                                     cur.foreach_fanin(m,
                                                       [&cur, &n1, &n2, &m, &is_y, &keeps_fanout](const auto& f)
                                                       {
                                                           const auto fn = cur.get_node(f);
                                                           if (!cur.is_constant(fn) &&
                                                               !((m == n1 || m == n2) && is_y(fn)))
                                                           {
                                                               keeps_fanout.insert(fn);
                                                           }
                                                       });
                                 });

        // levels up to the one the shared node goes into are copied as they are
        new_ranks.push_back(map_rank(cur.get_ranks(0)));

        const auto last_verbatim = insert_level ? r - 1 : r - 2;

        for (uint32_t l = 1; l <= last_verbatim; ++l)
        {
            const auto old_rank = cur.get_ranks(l);
            std::ranges::for_each(old_rank, clone);
            new_ranks.push_back(map_rank(old_rank));
        }

        // the level of the shared node: new nodes with the position that orders them
        struct placed
        {
            node   n;
            double key;
        };

        std::vector<placed> shared_level{};
        // maps a node of level r - 1 to the signal that replaces it as a fanin of level r
        mockturtle::node_map<signal, ranked_network> replaced_by{cur};
        std::vector<uint32_t>                        h_positions{};
        std::vector<signal>                          h_children{};

        if (insert_level)
        {
            cur.foreach_node_in_rank(r - 1,
                                     [&cur, &dest, &old2new, &keeps_fanout, &replaced_by, &shared_level](const auto& f)
                                     {
                                         if (keeps_fanout.contains(f))
                                         {
                                             replaced_by[f] = dest.create_buf(old2new[f]);
                                             shared_level.push_back({dest.get_node(replaced_by[f]),
                                                                     static_cast<double>(cur.rank_position(f))});
                                         }
                                     });

            for (const auto& y : dec.h_fanins)
            {
                h_children.push_back(old2new[y]);
                h_positions.push_back(cur.rank_position(y));
            }
        }
        else
        {
            cur.foreach_node_in_rank(
                r - 1,
                [&cur, &dest, &old2new, &is_y, &keeps_fanout, &replaced_by, &shared_level, &clone](const auto& q)
                {
                    // a buffer whose only fanouts were the Y edges disappears with them
                    if (is_y(q) && !keeps_fanout.contains(q))
                    {
                        return;
                    }
                    clone(q);
                    replaced_by[q] = old2new[q];
                    shared_level.push_back({dest.get_node(old2new[q]), static_cast<double>(cur.rank_position(q))});
                });

            for (const auto& y : dec.h_fanins)
            {
                assert(cur.is_buf(y) && "only buffers of the inserted level feed a shared node placed there");
                // the source of the buffer sits in level r - 2 and is mapped already
                cur.foreach_fanin(y, [&cur, &old2new, &h_children](const auto& f)
                                  { h_children.push_back(old2new[cur.get_node(f)]); });
                h_positions.push_back(cur.rank_position(y));
            }
        }

        const auto h = dest.create_node(h_children, dec.h);
        shared_level.push_back({dest.get_node(h), median_position(h_positions)});

        std::ranges::stable_sort(shared_level, [](const auto& a, const auto& b) { return a.key < b.key; });

        {
            std::vector<node> level_nodes{};
            level_nodes.reserve(shared_level.size());
            std::ranges::transform(shared_level, std::back_inserter(level_nodes), [](const auto& p) { return p.n; });
            new_ranks.push_back(std::move(level_nodes));
        }

        // level r: the pair becomes g1 and g2 over the shared node, every other node is copied over the replacements
        const auto new_lut =
            [&dest, &replaced_by, &h](const std::vector<node>& own_fanins, const kitty::dynamic_truth_table& g)
        {
            std::vector<signal> children{};
            children.reserve(own_fanins.size() + 1);
            std::ranges::transform(own_fanins, std::back_inserter(children),
                                   [&replaced_by](const auto& z) { return replaced_by[z]; });
            children.push_back(h);
            return dest.create_node(children, g);
        };

        const auto old_rank_r = cur.get_ranks(r);

        for (const auto& m : old_rank_r)
        {
            if (m == n1)
            {
                old2new[m] = new_lut(dec.g1_fanins, dec.g1);
            }
            else if (m == n2)
            {
                old2new[m] = new_lut(dec.g2_fanins, dec.g2);
            }
            else
            {
                std::vector<signal> children{};
                children.reserve(cur.fanin_size(m));
                cur.foreach_fanin(m,
                                  [&cur, &old2new, &replaced_by, &children](const auto& f)
                                  {
                                      const auto fn = cur.get_node(f);
                                      children.push_back(cur.is_constant(fn) ? old2new[fn] : replaced_by[fn]);
                                  });
                old2new[m] = dest.clone_node(cur, m, children);
            }
        }

        new_ranks.push_back(map_rank(old_rank_r));

        // levels above are copied as they are
        for (uint32_t l = r + 1; l <= cur.depth(); ++l)
        {
            const auto old_rank = cur.get_ranks(l);
            std::ranges::for_each(old_rank, clone);
            new_ranks.push_back(map_rank(old_rank));
        }

        cur.foreach_po([&cur, &dest, &old2new](const auto& po) { dest.create_po(old2new[cur.get_node(po)]); });

        ranked_network result{dest, new_ranks};

        assert(result.depth() + 1 == new_ranks.size() && "the derived order must cover every level");
        assert(result.check_validity() && "the derived order must match the levels");
        assert(is_balanced(result) && "the result must stay balanced");

        return result;
    }

    /**
     * Source network.
     */
    const Ntk& ntk;
    /**
     * Parameters.
     */
    const crossing_decomposition_params& ps;
    /**
     * Statistics.
     */
    crossing_decomposition_stats& pst;
};

}  // namespace detail

/**
 * Decomposes pairs of LUTs that share fanins into a shared node whenever that lowers the number of edge crossings of
 * the ranked network. The network is ranked, ordered once with `mincross`, and then improved locally: existing ranks
 * are never re-sorted, a decomposition is accepted only if the crossing count drops.
 *
 * This is the workflow of \"Reducing Wire Crossings in Field-Coupled Nanotechnologies\" by B. Hien, M. Walter, and
 * R. Wille in IEEE NANO 2024, re-implemented on top of `mincross`.
 *
 * @tparam Ntk Balanced LUT network type with node functions, e.g., `technology_network`.
 * @param ntk Balanced network.
 * @param ps Parameters.
 * @param pst Statistics.
 * @return The ranked and ordered network with the accepted decompositions applied.
 * @throws std::invalid_argument If `ntk` is not balanced.
 */
template <typename Ntk>
[[nodiscard]] fiction::networks::views::mutable_rank_view<Ntk>
crossing_decomposition(const Ntk& ntk, const crossing_decomposition_params& ps = {},
                       crossing_decomposition_stats* pst = nullptr)
{
    static_assert(mockturtle::is_network_type_v<Ntk>, "Ntk is not a network type");
    static_assert(mockturtle::has_node_function_v<Ntk>, "Ntk does not implement the node_function method");
    static_assert(mockturtle::has_create_node_v<Ntk>, "Ntk does not implement the create_node method");
    static_assert(mockturtle::has_create_buf_v<Ntk>, "Ntk does not implement the create_buf method");
    static_assert(mockturtle::has_clone_node_v<Ntk>, "Ntk does not implement the clone_node method");
    static_assert(mockturtle::has_is_buf_v<Ntk>, "Ntk does not implement the is_buf method");

    if (!is_balanced(ntk))
    {
        throw std::invalid_argument("The network must be balanced before crossing decomposition");
    }

    crossing_decomposition_stats st{};

    detail::crossing_decomposition_impl<Ntk> p{ntk, ps, st};

    auto result = [&p, &st]()
    {
        const mockturtle::stopwatch stop{st.time_total};
        return p.run();
    }();

    if (pst != nullptr)
    {
        *pst = st;
    }

    return result;
}

}  // namespace fiction::synthesis
