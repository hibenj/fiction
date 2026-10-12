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
#include <mockturtle/traits.hpp>
#include <mockturtle/utils/stopwatch.hpp>

#include <cstdint>
#include <iostream>
#include <stdexcept>

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
