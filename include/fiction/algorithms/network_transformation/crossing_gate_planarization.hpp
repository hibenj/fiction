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
#include <limits>
#include <map>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

namespace fiction
{

enum crossing_gate_planarization_params
{

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

                    fanout_ntk.foreach_fanout(n, [&](auto const& fo) { targets.emplace_back(edge{n, fo}); });

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

                                result.crossings.emplace_back(e, prev_edge, level);
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
        auto  init          = mockturtle::initialize_copy_network<Ntk>(ntk);
        auto& ntk_dest      = init.first;
        auto& old2new       = init.second;

        // crossing information
        ncross_extended();
        print_crossings();

        // mapping: edge -> crossing node created in ntk_dest
        std::unordered_map<edge, typename Ntk::node, edge_hash> edge_to_cross_node{};
        std::unordered_map<edge, typename Ntk::node, edge_hash> edge_to_buf_node{};

        auto place_crossing = [&](auto const& item)
        {
            auto const& e1 = item.e1;
            auto const& e2 = item.e2;

            auto it1 = edge_to_cross_node.find(e1);
            auto it2 = edge_to_cross_node.find(e2);

            typename Ntk::node parent1 = (it1 != edge_to_cross_node.end()) ? it1->second : old2new[e1.source];

            typename Ntk::node parent2 = (it2 != edge_to_cross_node.end()) ? it2->second : old2new[e2.source];

            auto sig1 = ntk_dest.make_signal(parent1);
            auto sig2 = ntk_dest.make_signal(parent2);

            // 3-XOR crossing construction
            auto c0 = ntk_dest.create_xor(sig1, sig2);
            /*sig1 = ntk_dest.create_buf(sig1);
            sig2 = ntk_dest.create_buf(sig2);*/
            auto c2 = ntk_dest.create_xor(c0, sig2);  // e1 path
            auto c1 = ntk_dest.create_xor(sig1, c0);  // e2 path

            // update mapping for future chaining
            edge_to_cross_node[e1] = c2;
            edge_to_cross_node[e2] = c1;

            std::cout << "place crossing: (" << e1.source << " -> " << e1.target << ")  x  (" << e2.source << " -> "
                      << e2.target << ")\n";
        };

        // === process per rank (edges are between rank r-1 and r) ===
        for (uint32_t r = 1u; r < fanout_ntk.depth() + 1; ++r)
        {
            // crossings between rank r-1 and r
            auto ordered = crossing_ctn[r - 1].crossings;  // copy so we can sort

            // sort by level
            std::sort(ordered.begin(), ordered.end(), [](auto const& a, auto const& b) { return a.level < b.level; });

            auto const& unaffected = crossing_ctn[r - 1].unaffected;
            size_t      it         = 0;

            auto extra_depth = crossing_ctn[r - 1].crossings_per_level.rbegin()->first + 1;

            // === Case 1: no unaffected edges ===
            if (unaffected.empty())
            {
                while (it < ordered.size())
                {
                    place_crossing(ordered[it++]);
                }
            }
            else
            {
                auto const& first_unaf = unaffected.front();

                while (it < ordered.size() &&
                       (first_unaf.source >= ordered[it].e1.source && first_unaf.target >= ordered[it].e1.target))
                {
                    place_crossing(ordered[it++]);
                }

                // === Main merge loop ===
                for (size_t i = 0; i < unaffected.size(); ++i)
                {
                    auto const& curr_edge = unaffected[i];
                    std::cout << "place unaffected edge: " << curr_edge.source << ", " << curr_edge.target << "\n";

                    // start from the current mapped source node
                    auto last = old2new[curr_edge.source];

                    // create a chain of `extra_depth` buffers
                    // ToDo: Modify for the number of gates for one crossing
                    const uint32_t crossing_depth = 2;
                    for (uint64_t j = 0; j < extra_depth * crossing_depth; ++j)
                    {
                        last = ntk_dest.create_buf(last);
                        std::cout << "Buffer\n";
                    }

                    // the last buffer drives the target
                    edge_to_buf_node[curr_edge] = last;

                    uint64_t next_src =
                        (i + 1 < unaffected.size()) ? unaffected[i + 1].source : std::numeric_limits<uint64_t>::max();
                    uint64_t next_tgt =
                        (i + 1 < unaffected.size()) ? unaffected[i + 1].target : std::numeric_limits<uint64_t>::max();

                    while (it < ordered.size())
                    {
                        // if the next crossing is bigger than my next unaffected edge, then break
                        if (ordered[it].e1.source >= next_src && ordered[it].e1.target >= next_tgt)
                        {
                            break;
                        }

                        // otherwise, place this crossing and move to the next
                        place_crossing(ordered[it]);
                        ++it;
                    }
                }
            }

            // === now copy logic nodes in this rank ===
            ntk.foreach_node_in_rank(
                r,
                [&](auto const& g)
                {
                    if (ntk.is_constant(g) || ntk.is_ci(g))
                    {
                        return;
                    }

                    std::vector<typename Ntk::signal> children{};

                    const auto& unaffected_edges = crossing_ctn[r - 1].unaffected;

                    ntk.foreach_fanin(g,
                                      [&](auto const& f)
                                      {
                                          auto fn = ntk.get_node(f);
                                          edge e{fn, g};

                                          typename Ntk::signal tgt_signal;

                                          auto it_buf = edge_to_buf_node.find(e);
                                          if (it_buf != edge_to_buf_node.end())
                                          {
                                              tgt_signal = ntk_dest.make_signal(it_buf->second);
                                          }
                                          else
                                          {
                                              // affected edge: use crossing node if present, fall back to old2new
                                              auto it_cross = edge_to_cross_node.find(e);

                                              if (it_cross != edge_to_cross_node.end())
                                              {
                                                  tgt_signal = ntk_dest.make_signal(it_cross->second);
                                              }
                                              else
                                              {
                                                  tgt_signal = old2new[fn];
                                              }
                                          }

                                          if (ntk.is_complemented(f))
                                          {
                                              tgt_signal = ntk_dest.create_not(tgt_signal);
                                          }

                                          children.emplace_back(tgt_signal);
                                      });

                    // === create node in destination network ===
                    if constexpr (mockturtle::has_node_function_v<Ntk> && mockturtle::has_create_node_v<Ntk>)
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
    /*mincross_stats  st_min{};
    mincross_params p_min{};
    p_min.optimize = false;

    auto ntk_min = mincross(result, p_min, &st_min);  // counts crossings
    std::cout << "Crossings: " << st_min.num_crossings << '\n';
    if (st_min.num_crossings != 0)
    {
        throw std::runtime_error("Planarization failed: resulting network is not planar");
    }*/

    return result;
}

}  // namespace fiction

#endif  // FICTION_CROSSING_GATE_PLANARIZATION_HPP
