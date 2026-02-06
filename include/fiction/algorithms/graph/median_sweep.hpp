//
// Single top-down median sweep (POs to PIs) for leveled networks.
//

#ifndef FICTION_MEDIAN_SWEEP_HPP
#define FICTION_MEDIAN_SWEEP_HPP

#include <mockturtle/traits.hpp>
#include <mockturtle/views/fanout_view.hpp>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace fiction
{

/**
 * Perform a single median-based sweep from outputs toward inputs.
 *
 * The procedure mirrors Graphviz's downward pass: for each rank r (from top to bottom),
 * it reorders the nodes in rank r using the medians of their neighbors in rank r + 1.
 * Only one sweep is performed; no transposition or iterative refinement is done.
 *
 * @tparam Ntk Balanced logic network type with rank accessors.
 * @param ntk Network to reorder in place.
 */
template <typename Ntk>
void median_sweep_pos_to_pis(Ntk& ntk)
{
    static_assert(mockturtle::is_network_type_v<Ntk>, "Ntk is not a network type");
    static_assert(mockturtle::has_rank_position_v<Ntk>, "Ntk does not implement the rank_position function");

    // fanout_view gives convenient fanout traversal and rank helpers
    mockturtle::fanout_view<Ntk> fanout_ntk{ntk};

    using node = typename Ntk::node;

    // No sweep needed for a single rank
    if (fanout_ntk.depth() == 0)
    {
        return;
    }

    std::unordered_map<node, double> median_map;

    const auto compute_median = [&fanout_ntk](const node& n, const uint32_t target_rank) {
        std::vector<uint32_t> positions{};
        positions.reserve(fanout_ntk.fanout_size(n));

        fanout_ntk.foreach_fanout(
            n,
            [&fanout_ntk, &positions, target_rank](auto const& fo)
            {
                if (fanout_ntk.level(fo) == target_rank)
                {
                    positions.push_back(fanout_ntk.rank_position(fo));
                }
            });

        if (positions.empty())
        {
            return -1.0;  // no neighbors in the next rank; push to the back
        }

        std::sort(positions.begin(), positions.end());

        const auto sz = positions.size();
        if (sz == 1)
        {
            return static_cast<double>(positions[0]);
        }
        if (sz == 2)
        {
            return static_cast<double>(positions[0] + positions[1]) / 2.0;
        }

        if (sz % 2 == 1)
        {
            return static_cast<double>(positions[sz / 2]);
        }

        const std::size_t rm    = sz / 2;
        const std::size_t lm    = rm - 1;
        const uint32_t    lspan = positions[lm] - positions.front();
        const uint32_t    rspan = positions.back() - positions[rm];

        if (lspan == rspan)
        {
            return static_cast<double>(positions[lm] + positions[rm]) / 2.0;
        }

        const double w = (positions[lm] * static_cast<double>(rspan)) +
                         (positions[rm] * static_cast<double>(lspan));
        return w / static_cast<double>(lspan + rspan);
    };

    const uint32_t max_rank = fanout_ntk.depth();

    // Sweep downward: reorder rank r based on neighbors in rank r + 1
    for (uint32_t r = max_rank - 1; r != static_cast<uint32_t>(-1); --r)
    {
        const uint32_t next_rank = r + 1;

        auto rank_nodes = fanout_ntk.get_ranks(r);
        median_map.clear();
        median_map.reserve(rank_nodes.size());

        for (auto const& n : rank_nodes)
        {
            median_map[n] = compute_median(n, next_rank);
        }

        std::sort(rank_nodes.begin(), rank_nodes.end(), [&median_map](auto const& a, auto const& b) {
            const double ma = median_map[a];
            const double mb = median_map[b];

            // nodes without neighbors (-1) go last
            if (ma == -1.0)
            {
                return false;
            }
            if (mb == -1.0)
            {
                return true;
            }
            return ma < mb;
        });

        fanout_ntk.set_ranks(r, rank_nodes);
        ntk.set_ranks(r, rank_nodes);  // keep underlying network consistent
    }
}

}  // namespace fiction

#endif  // FICTION_MEDIAN_SWEEP_HPP

