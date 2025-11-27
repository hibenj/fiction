//
// Created by benjamin on 25.11.25.
//

#include <catch2/catch_test_macros.hpp>

#include "fiction/networks/technology_network.hpp"
#include "fiction/networks/views/mutable_rank_view.hpp"
#include "fiction/utils/debug/network_writer.hpp"

#include <fiction/algorithms/network_transformation/crossing_gate_planarization.hpp>

#include <mockturtle/algorithms/equivalence_checking.hpp>
#include <mockturtle/algorithms/miter.hpp>

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <vector>

using namespace fiction;

TEST_CASE("Hello World", "[crossing-gate-planarization]")
{
    technology_network tec{};
    const auto         pi1 = tec.create_pi();
    const auto         pi2 = tec.create_pi();
    const auto         pi3 = tec.create_pi();
    const auto         a1  = tec.create_and(pi1, pi2);
    const auto         o1  = tec.create_or(pi1, pi2);
    const auto         a2  = tec.create_and(pi3, pi1);
    tec.create_po(a1);
    tec.create_po(o1);
    tec.create_po(a2);

    debug::write_dot_network(tec);

    auto       tec_r = mutable_rank_view(tec);
    const auto tec_p = crossing_gate_planarization(tec_r);

    debug::write_dot_network(tec_p, "planarized");
}

TEST_CASE("Test Benes", "[crossing-gate-planarization]")
{
    const uint64_t                                          n        = 3;
    const uint64_t                                          N        = 1ULL << n;
    const uint64_t                                          stages   = 2 * n - 1;
    const uint64_t                                          elements = N / 2;
    std::vector<std::vector<std::pair<uint64_t, uint64_t>>> connections(stages - 1);

    for (uint64_t i = 0; i < (stages - 1) / 2; ++i)
    {
        const uint64_t step = elements >> i;
        const uint64_t half = step >> static_cast<uint64_t>(1);

        for (uint64_t g = 0; g < elements / step; ++g)
        {
            for (uint64_t j = 0; j < half; ++j)
            {
                const uint64_t a = g * step + j + 1;
                const uint64_t b = a + half;

                const auto p = std::make_pair(a, b);
                connections[i].push_back(p);
                connections[i].push_back(p);
            }
        }
    }
    auto build_inverse_stage = [&](const std::vector<std::pair<uint64_t, uint64_t>>& forward,
                                   std::vector<std::pair<uint64_t, uint64_t>>&       backward)
    {
        backward.clear();
        const uint64_t                     m = forward.size();
        std::vector<std::vector<uint64_t>> pos(m + 1);

        for (uint64_t idx = 0; idx < m; ++idx)
        {
            const auto [a, b] = forward[idx];
            pos[a].push_back(idx + 1);
            pos[b].push_back(idx + 1);
        }

        for (uint64_t u = 1; u <= m; ++u)
        {
            if (pos[u].size() >= 2)
            {
                std::sort(pos[u].begin(), pos[u].end());
                backward.push_back({pos[u][0], pos[u][1]});
            }
        }
    };

    for (uint64_t i = 0; i < (stages - 1) / 2; ++i)
    {
        build_inverse_stage(connections[i], connections[stages - 2 - i]);
    }

    technology_network tec{};

    // 1. Create all primary inputs
    std::vector<mockturtle::signal<technology_network>> current_nodes;
    current_nodes.reserve(elements);
    for (uint64_t i = 0; i < elements; ++i)
    {
        current_nodes.push_back(tec.create_pi());
    }

    // 2. Build network layer by layer (from back to front)
    for (auto stage = static_cast<int64_t>(stages) - 2; stage >= 0; --stage)
    {
        // std::cout << "Stage: " << stage << '\n';
        std::vector<mockturtle::signal<technology_network>> next_nodes;
        next_nodes.reserve(elements);

        for (uint64_t e = 0; e < elements; ++e)
        {
            const auto [a, b] = connections[stage][e];

            const auto fanin0 = current_nodes[a - 1];
            const auto fanin1 = current_nodes[b - 1];

            const auto node = tec.create_and(fanin0, fanin1);
            next_nodes.push_back(node);
        }

        current_nodes = std::move(next_nodes);
    }

    for (auto const& po : current_nodes)
    {
        tec.create_po(po);
    }

    debug::write_dot_network(tec);

    auto       tec_r = mutable_rank_view(tec);
    const auto tec_p = crossing_gate_planarization(tec_r);

    const auto miter = mockturtle::miter<technology_network>(tec, tec_p);
    const auto eq    = mockturtle::equivalence_checking(*miter);

    CHECK(eq == 1);

    debug::write_dot_network(tec_p, "planarized");
}