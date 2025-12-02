//
// Created by benjamin on 25.11.25.
//
#ifndef FICTION_CROSSING_GATE_PLANARIZATION_HPP
#define FICTION_CROSSING_GATE_PLANARIZATION_HPP

#include "fiction/algorithms/graph/mincross.hpp"
#include "fiction/algorithms/network_transformation/network_balancing.hpp"

#include <mockturtle/traits.hpp>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

namespace fiction
{

struct crossing_gate_planarization_params
{
    enum on_off : std::uint8_t
    {
        ON,
        OFF
    };

    on_off mode = ON;
    on_off verbose = OFF;
};

namespace detail
{

template <typename Ntk>
class crossing_gate_planarization_impl
{
  public:
    [[maybe_unused]] crossing_gate_planarization_impl(const Ntk& src, const crossing_gate_planarization_params& p) :
            ntk(src),
            fanout_ntk(src),
            ps{p}
    {}

    struct edge
    {
        mockturtle::node<Ntk> source;
        mockturtle::node<Ntk> target;

        bool operator==(edge const& other) const
        {
            return (source == other.source) && (target == other.target);
        }
    };

    struct crossing_item
    {
        edge     e1;
        edge     e2;
        uint64_t level;

        crossing_item(edge const& _e1, edge const& _e2, uint64_t _lvl) : e1(_e1), e2(_e2), level(_lvl) {}
    };

    struct stage_result
    {
        std::vector<crossing_item>   crossings;
        std::vector<edge>            unaffected;
        std::vector<edge>            edges;
        std::map<uint64_t, uint64_t> crossings_per_level;
    };

    struct edge_hash
    {
        std::size_t operator()(edge const& e) const noexcept
        {
            // simple but effective: mix source + target
            return std::hash<mockturtle::node<Ntk>>{}(e.source) ^ (std::hash<mockturtle::node<Ntk>>{}(e.target) << 1);
        }
    };

    // ============================================================
    // PRINT CROSSINGS
    // ============================================================

    void print_crossings() const
    {
        std::cout << "Total stages: " << crossing_ctn.size() << "\n";

        for (size_t r = 0; r < crossing_ctn.size(); ++r)
        {
            std::cout << "\n=== Stage " << r << " ===\n";

            std::cout << "Crossings:\n";
            for (auto const& cr : crossing_ctn[r].crossings)
            {
                auto const& e1    = cr.e1;
                auto const& e2    = cr.e2;
                auto const  level = cr.level;

                std::cout << "  L" << level << ": (" << fanout_ntk.node_to_index(e1.source) << " -> "
                          << fanout_ntk.node_to_index(e1.target) << ")  x  (" << fanout_ntk.node_to_index(e2.source)
                          << " -> " << fanout_ntk.node_to_index(e2.target) << ")\n";
            }

            std::cout << "Unaffected edges: ";
            for (auto const& u : crossing_ctn[r].unaffected)
            {
                std::cout << "(" << fanout_ntk.node_to_index(u.source) << " -> " << fanout_ntk.node_to_index(u.target)
                          << ") ";
            }
            std::cout << "\n";

            std::cout << "Crossings per level:\n";
            for (auto const& p : crossing_ctn[r].crossings_per_level)
            {
                std::cout << "  L" << p.first << ": " << p.second << "\n";
            }
        }
    }

    // ============================================================
    // CROSSING DETECTION (ncross_extended)
    // ============================================================

    void ncross_extended()
    {
        total_crossings = 0;
        crossing_ctn.clear();

        for (uint32_t r = 0u; r < fanout_ntk.depth(); ++r)
        {
            uint64_t next_width = fanout_ntk.rank_width(r + 1);

            std::vector<std::deque<std::pair<edge, uint64_t>>> penalty(next_width + 1);
            uint64_t                                           max_pos = 0;

            stage_result      result{};
            std::vector<edge> affected_edges;
            std::vector<edge> stage_edges;

            fanout_ntk.foreach_node_in_rank(
                r,
                [this, &penalty, &max_pos, &result, &affected_edges, &stage_edges](auto const& n)
                {
                    std::vector<edge> targets;
                    targets.reserve(fanout_ntk.fanout_size(n));

                    fanout_ntk.foreach_fanout(n,
                                              [&](auto const& fo)
                                              {
                                                  auto e = edge{n, fo};
                                                  targets.emplace_back(e);
                                                  result.edges.emplace_back(e);
                                              });

                    for (auto const& e : targets)
                    {
                        uint64_t pos       = fanout_ntk.rank_position(e.target);
                        uint64_t local_lvl = 0;

                        for (auto k = static_cast<uint64_t>(max_pos); k >= static_cast<uint64_t>(pos + 1); --k)
                        {
                            // iterate newest edges first
                            for (auto it = penalty[k].begin(); it != penalty[k].end(); ++it)
                            {
                                auto const& prev_edge = it->first;
                                auto&       prev_lvl  = it->second;

                                uint64_t level = std::max(local_lvl, prev_lvl);

                                result.crossings.emplace_back(prev_edge, e, level);
                                result.crossings_per_level[level]++;

                                affected_edges.push_back(e);
                                affected_edges.push_back(prev_edge);

                                if (prev_lvl > local_lvl)
                                {
                                    local_lvl = prev_lvl;
                                }

                                prev_lvl++;
                                total_crossings++;
                                local_lvl++;
                            }
                        }
                    }

                    // Insert into penalty
                    for (auto const& e : targets)
                    {
                        uint64_t pos = fanout_ntk.rank_position(e.target);
                        max_pos      = std::max(max_pos, pos);

                        penalty[pos].push_front({e, 0});
                        stage_edges.push_back(e);
                    }
                });

            for (auto const& e : stage_edges)
            {
                if (std::find(affected_edges.begin(), affected_edges.end(), e) == affected_edges.end())
                {
                    result.unaffected.push_back(e);
                }
            }

            crossing_ctn.push_back(std::move(result));
        }
    }

    Ntk run()
    {
        auto  init     = mockturtle::initialize_copy_network<Ntk>(ntk);
        auto& ntk_dest = init.first;
        auto& old2new  = init.second;

        // crossing information
        ncross_extended();
        if (ps.verbose == crossing_gate_planarization_params::on_off::ON)
        {
            print_crossings();
        }

        // mapping: edge -> crossing node created in ntk_dest
        std::unordered_map<edge, typename Ntk::node, edge_hash> edge_to_node{};

        // lambda to create buffer chain for a given edge
        auto create_buffer_chain = [&](auto const& edge)
        {
            typename Ntk::node initial;

            // check if we already buffered this edge
            auto it = edge_to_node.find(edge);
            if (it != edge_to_node.end())
            {
                initial = it->second;  // continue from last buffer chain node
            }
            else
            {
                initial = old2new[edge.source];  // start from mapped original node
            }

            auto           last           = initial;
            const uint32_t crossing_depth = 2;  // TODO: adjust for number of gates per crossing

            for (uint32_t j = 0; j < crossing_depth; ++j)
            {
                last = ntk_dest.create_buf(last);
            }

            // update mapping
            edge_to_node[edge] = last;

            return last;
        };

        auto place_crossing = [&](auto const& item)
        {
            auto const& e1 = item.e1;
            auto const& e2 = item.e2;

            auto it1 = edge_to_node.find(e1);
            auto it2 = edge_to_node.find(e2);

            typename Ntk::node child1 = (it1 != edge_to_node.end()) ? it1->second : old2new[e1.source];

            typename Ntk::node child2 = (it2 != edge_to_node.end()) ? it2->second : old2new[e2.source];

            auto sig1 = ntk_dest.make_signal(child1);
            auto sig2 = ntk_dest.make_signal(child2);

            // sig3 indicates buffered ON-mode (will remain empty in OFF mode)
            mockturtle::signal<Ntk> sig3{};

            if (ps.mode == crossing_gate_planarization_params::on_off::ON)
            {
                // Must be first in ON mode
                sig3 = ntk_dest.create_buf(sig1);   // buffer sig1 before XOR
            }

            // Always executed next
            auto c0 = ntk_dest.create_xor(sig1, sig2);

            // Remaining logic depends on whether sig3 was created or not
            mockturtle::signal<Ntk> c1{};
            mockturtle::signal<Ntk> c2{};

            if (sig3)  // ON mode, sig3 is a real node
            {
                sig2 = ntk_dest.create_buf(sig2);   // must happen after c0
                c1   = ntk_dest.create_xor(sig3, c0);
                c2   = ntk_dest.create_xor(c0, sig2);
            }
            else // OFF mode
            {
                c1 = ntk_dest.create_xor(sig1, c0);
                c2 = ntk_dest.create_xor(c0, sig2);
            }

            // update mapping for future chaining
            edge_to_node[e1] = c2;
            edge_to_node[e2] = c1;
        };

        // === process per rank (edges are between rank r-1 and r) ===
        for (uint32_t r = 1u; r < fanout_ntk.depth() + 1; ++r)
        {
            // crossings between rank r-1 and r
            auto ordered = crossing_ctn[r - 1].crossings;  // copy so we can sort

            // sort by level
            std::sort(ordered.begin(), ordered.end(), [](auto const& a, auto const& b) { return a.level < b.level; });

            uint32_t cross_it = 0;
            auto&    edges    = crossing_ctn[r - 1].edges;

            while (true)
            {
                for (size_t i = 0; i < edges.size(); ++i)
                {
                    auto e                  = edges[i];
                    auto next_crossing_edge = ordered[cross_it].e1;

                    if (next_crossing_edge == e)
                    {
                        assert(i + 1 < edges.size());
                        place_crossing(ordered[cross_it++]);

                        // swap edges[i] and edges[i+1]
                        std::swap(edges[i], edges[i + 1]);

                        ++i;  // skip next element since we swapped and placed
                    }
                    else if (ps.mode == crossing_gate_planarization_params::on_off::ON)
                    {
                        create_buffer_chain(e);
                    }
                }

                // if no crossings placed during loop --> layer is stable, stop
                if (cross_it >= ordered.size())
                {
                    break;
                }
            }

            // === now copy logic nodes in this rank ===
            ntk.foreach_node_in_rank(r,
                                     [&](auto const& g)
                                     {
                                         if (ntk.is_constant(g) || ntk.is_ci(g))
                                         {
                                             return;
                                         }

                                         std::vector<typename Ntk::signal> children{};

                                         ntk.foreach_fanin(g,
                                                           [&](auto const& f)
                                                           {
                                                               auto fn = ntk.get_node(f);
                                                               edge e{fn, g};

                                                               typename Ntk::signal tgt_signal;

                                                               auto it = edge_to_node.find(e);
                                                               if (it != edge_to_node.end())
                                                               {
                                                                   tgt_signal = ntk_dest.make_signal(it->second);
                                                               }
                                                               else
                                                               {
                                                                   tgt_signal = old2new[fn];
                                                               }

                                                               if (ntk.is_complemented(f))
                                                               {
                                                                   tgt_signal = ntk_dest.create_not(tgt_signal);
                                                               }

                                                               children.emplace_back(tgt_signal);
                                                           });

                                         // === create node in destination network ===
                                         if constexpr (mockturtle::has_node_function_v<Ntk> &&
                                                       mockturtle::has_create_node_v<Ntk>)
                                         {
                                             old2new[g] = ntk_dest.create_node(children, ntk.node_function(g));
                                             return;
                                         }
                                     });
        }

        // create destination POs
        ntk.foreach_po(
            [this, &ntk_dest, &old2new](const auto& po)
            {
                const auto tgt_signal = old2new[ntk.get_node(po)];
                const auto tgt_po     = ntk.is_complemented(po) ? ntk_dest.create_not(tgt_signal) : tgt_signal;

                ntk_dest.create_po(tgt_po);
            });

        // restore signal names if applicable
        fiction::restore_names(ntk, ntk_dest, old2new);

        ntk_dest.update_ranks();

        return ntk_dest;
    }

  private:
    Ntk                                ntk{};
    mockturtle::fanout_view<Ntk>       fanout_ntk{};
    crossing_gate_planarization_params ps{};
    uint64_t                           total_crossings{0};
    std::vector<stage_result>          crossing_ctn{};
};

}  // namespace detail

template <typename Ntk>
[[nodiscard]] Ntk crossing_gate_planarization(const Ntk& ntk, crossing_gate_planarization_params ps = {})
{
    static_assert(mockturtle::is_network_type_v<Ntk>, "NtkSrc is not a network type");
    static_assert(mockturtle::has_create_node_v<Ntk>, "NtkSrc does not implement the create_node function");
    static_assert(mockturtle::has_rank_position_v<Ntk>, "NtkSrc does not implement the rank_position function");

    assert(ntk.is_combinational() && "Network has to be combinational");

    if (!is_balanced(ntk))
    {
        throw std::invalid_argument("Networks have to be balanced for this duplication");
    }

    detail::crossing_gate_planarization_impl p{ntk, ps};

    auto result = p.run();

    // check for planarity
    if (ps.mode == crossing_gate_planarization_params::on_off::ON)
    {
        mincross_stats  st_min{};
        mincross_params p_min{};
        p_min.optimize = false;

        auto ntk_min = mincross(result, p_min, &st_min);  // counts crossings
        std::cout << "Crossings: " << st_min.num_crossings << '\n';
        if (st_min.num_crossings != 0)
        {
            throw std::runtime_error("Planarization failed: resulting network is not planar");
        }
    }

    return result;
}

}  // namespace fiction

#endif  // FICTION_CROSSING_GATE_PLANARIZATION_HPP
