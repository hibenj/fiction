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
 * Implementation of the crossing-aware LUT decomposition.
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
     * Runs the algorithm: ranks the network, orders it with `mincross`, and records the crossing count. The
     * decomposition itself follows in a later slice; until then, the ordered network is returned unchanged.
     *
     * @return The ordered network.
     */
    [[nodiscard]] ranked_network run()
    {
        ranked_network ranked{ntk};

        fiction::utils::graph::mincross_stats mincross_st{};

        auto ordered = fiction::utils::graph::mincross(ranked, ps.mincross, &mincross_st);

        pst.crossings_before = mincross_st.num_crossings;
        pst.crossings_after  = pst.crossings_before;

        return ordered;
    }

  private:
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
