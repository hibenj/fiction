#include "fiction/networks/views/mutable_rank_view.hpp"
#include "fiction/utils/debug/network_writer.hpp"
#include "fiction_experiments.hpp"

#include <fiction/algorithms/graph/mincross.hpp>
#include <fiction/networks/technology_network.hpp>  // technology-mapped network type

#include <fmt/format.h>

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>

using namespace fiction;
int main()  // NOLINT
{
    //Benes network
    // input
    const uint64_t n = 4;
    std::cout << "n = " << n << '\n';
    // number of inputs/outputs
    const uint64_t N = 1ULL << n;
    std::cout << "B = " << N << '\n';
    // stages
    const uint64_t stages = 2 * n - 1;
    std::cout << "stages = " << stages << '\n';
    // elements per stage
    const uint64_t elements = N / 2;
    std::cout << "elements = " << elements << '\n';
    //connections, the dimensions should be stages-1 and elements
    std::vector<std::vector<std::pair<uint64_t, uint64_t>>> connections(stages - 1);
    // we only have to iterate through half of them becauseafterwards the other half is just the inverse of this

    for (uint64_t i = 0; i < (stages - 1) / 2; ++i)
    {
        const uint64_t step = elements >> i;   // elements / 2^i
        const uint64_t half = step >> static_cast<uint64_t>(1);       // step / 2

        for (uint64_t g = 0; g < elements / step; ++g)          // number of blocks
        {
            for (uint64_t j = 0; j < half; ++j)                 // pairs inside a block
            {
                const uint64_t a = g * step + j + 1;            // 1-based
                const uint64_t b = a + half;

                const auto p = std::make_pair(a, b);
                connections[i].push_back(p);
                connections[i].push_back(p); // you currently want duplicates
            }
        }
    }
    auto build_inverse_stage = [&](const std::vector<std::pair<uint64_t,uint64_t>>& forward,
                                   std::vector<std::pair<uint64_t,uint64_t>>& backward)
    {
        backward.clear();
        const uint64_t m = forward.size();         // == elements
        std::vector<std::vector<uint64_t>> pos(m + 1); // pos[node] -> list of entry indices (1-based)

        // Record which entry indices each node appears in
        for (uint64_t idx = 0; idx < m; ++idx) {
            const auto [a, b] = forward[idx];
            pos[a].push_back(idx + 1);  // entry positions are 1-based
            pos[b].push_back(idx + 1);
        }

        // For duplicates: each node should appear in exactly 2 entries -> connect those entry indices
        for (uint64_t u = 1; u <= m; ++u) {
            if (pos[u].size() >= 2) {
                std::sort(pos[u].begin(), pos[u].end());
                backward.push_back({pos[u][0], pos[u][1]});
            }
        }
    };

    // build second half as inverse connections
    for (uint64_t i = 0; i < (stages - 1) / 2; ++i) {
        build_inverse_stage(connections[i], connections[stages - 2 - i]);
    }

    /*for (uint64_t i = 0; i < (stages-1); ++i)
    {
        std::cout << "Stage: " << i << '\n';
        for (uint64_t e = 0; e < elements; ++e)
        {
            std::cout << "a: " << connections[i][e].first << ", b: " << connections[i][e].second << '\n';
        }
    }*/

    technology_network tec{};

    // 1. Create all primary inputs
    std::vector<mockturtle::signal<technology_network>> current_nodes;
    current_nodes.reserve(elements);
    for (uint64_t i = 0; i < elements; ++i)
        current_nodes.push_back(tec.create_pi());

    // 2. Build network layer by layer (from back to front)
    for (int64_t stage = static_cast<int64_t>(stages) - 2; stage >= 0; --stage)
    {
        // std::cout << "Stage: " << stage << '\n';
        std::vector<mockturtle::signal<technology_network>> next_nodes;
        next_nodes.reserve(elements);

        for (uint64_t e = 0; e < elements; ++e)
        {
            const auto [a, b] = connections[stage][e];
            // std::cout << "a: " << a << ", b: " << b << '\n';

            // Convert 1-based indices in your connections to 0-based vector indices
            const auto fanin0 = current_nodes[a - 1];
            const auto fanin1 = current_nodes[b - 1];

            // Create a simple AND node to represent the switch
            const auto node = tec.create_and(fanin0, fanin1);
            next_nodes.push_back(node);
        }

        // Move on to the next layer
        current_nodes = std::move(next_nodes);
    }

    // 3. Create primary outputs for the final layer
    for (auto const& n : current_nodes)
        tec.create_po(n);

    const auto tec_r = mutable_rank_view(tec);

    mincross_stats        st{};
    mincross_params p{};
    p.optimize = true;
    const auto            ntk = mincross(tec_r, p, &st);

    std::cout << "Number of crossings:" << st.num_crossings << '\n';

    debug::write_dot_network(tec);

    return EXIT_SUCCESS;
};