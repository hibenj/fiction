//
// Created by benjamin on 2/18/26.
//

#ifndef FICTION_CROSSING_GATE_PLANARIZATION_FO_HPP
#define FICTION_CROSSING_GATE_PLANARIZATION_FO_HPP

#include "fiction/algorithms/graph/mincross.hpp"
#include "fiction/algorithms/network_transformation/network_balancing.hpp"
#include "fiction/utils/debug/network_writer.hpp"

#include <mockturtle/traits.hpp>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <numeric>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace fiction
{

struct crossing_gate_planarization_params
{
    bool buffer    = true;
    bool verbose   = false;
    bool xor_gates = false;
};

namespace detail
{

template <typename Ntk>
[[nodiscard]] std::pair<Ntk, mockturtle::node_map<mockturtle::signal<Ntk>, Ntk>>
initialize_copy_virtual_pi_network(const Ntk& src)
{
    static_assert(mockturtle::has_rank_position_v<Ntk>, "NtkSrc does not implement the rank_position function");
    mockturtle::node_map<mockturtle::signal<Ntk>, Ntk> old2new(src);
    Ntk                                                dest;

    old2new[src.get_constant(false)] = dest.get_constant(false);
    if (src.get_node(src.get_constant(true)) != src.get_node(src.get_constant(false)))
    {
        old2new[src.get_constant(true)] = dest.get_constant(true);
    }

    // discard virtual PIs and the PI order in the extended rank view when creating the PIs for the copy network
    if constexpr (has_is_real_pi_v<Ntk>)
    {
        src.foreach_pi_unranked(
            [&](auto const& n)
            {
                if (src.is_real_pi(n))
                {
                    old2new[n] = dest.create_pi();
                }
                else
                {
                    old2new[n] = dest.create_virtual_pi(src.get_real_pi(n));
                }
            });
    }
    else
    {
        src.foreach_pi_unranked([&](auto const& n) { old2new[n] = dest.create_pi(); });
    }

    return {dest, old2new};
}

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

        bool operator!=(edge const& other) const
        {
            return !(*this == other);
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

    struct crossing_item_fo
    {
        std::vector<edge>        e1;
        std::vector<edge>        e2;
        std::vector<std::size_t> fo_crossings{};
        uint64_t                 level;

        crossing_item_fo(const edge& _e1, const edge& _e2, uint64_t _lvl) : e1{_e1}, e2{_e2}, level(_lvl) {}
    };

    struct stage_result_fo
    {
        std::vector<crossing_item_fo> crossings;
        std::vector<edge>             unaffected;
        std::vector<edge>             edges;
        std::map<uint64_t, uint64_t>  crossings_per_level;
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
        std::cout << "Total crossings: " << total_crossings << "\n";
        std::cout << "Total crossings fo: " << total_crossings_fo << "\n";
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
    bool ncross_fo()
    {
        total_crossings_fo = 0;
        crossing_ctn_fo.clear();

        for (uint32_t r = 0u; r < fanout_ntk.depth(); ++r)
        {
            uint32_t cross_limit = 1000u;
            // per-rank crossing limit check
            uint32_t rank_crossings = 0u;

            uint64_t next_width = fanout_ntk.rank_width(r + 1);

            std::vector<std::deque<std::pair<edge, uint64_t>>> penalty(next_width + 1);
            uint64_t                                           max_pos = 0;

            stage_result_fo   result{};
            std::vector<edge> affected_edges;
            std::vector<edge> stage_edges;

            fanout_ntk.foreach_node_in_rank(
                r,
                [this, &penalty, &max_pos, &result, &affected_edges, &stage_edges, &rank_crossings,
                 &cross_limit](auto const& n)
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

                    std::sort(targets.begin(), targets.end(), [this](const edge& a, const edge& b)
                              { return fanout_ntk.rank_position(a.target) < fanout_ntk.rank_position(b.target); });

                    std::vector<uint32_t> crossing_fo_iterator_same_penalty{};

                    for (auto const& e : targets)
                    {
                        uint64_t              pos = fanout_ntk.rank_position(e.target);
                        std::vector<uint32_t> crossing_fo_iterator_same_edge{};

                        for (auto k = max_pos; k >= pos + 1; --k)
                        {
                            for (auto it = penalty[k].begin(); it != penalty[k].end(); ++it)
                            {
                                auto const& prev_edge = it->first;
                                uint64_t    prev_lvl  = it->second;

                                // search, if there is already a crossing item with this
                                // edge and the same source
                                bool combine_crossings = false;
                                for (const auto& cross_it : crossing_fo_iterator_same_edge)
                                {
                                    // e2 container contains the same edge
                                    auto& prev_cross = result.crossings[cross_it];
                                    // check if e1 has the same source
                                    if (prev_cross.e1[0].source == prev_edge.source)
                                    {
                                        prev_cross.e1.insert(prev_cross.e1.begin(), prev_edge);
                                        for (const auto& e1 : prev_cross.e1)
                                        {
                                            const auto val = prev_cross.e1.size();
                                            auto&      vec = multi_signal[e1];  // creates empty vector if missing

                                            const auto target = val - 1;
                                            if (auto it_t = std::find(vec.begin(), vec.end(), target); it_t != vec.end())
                                            {
                                                ++(*it_t);
                                            }
                                            else
                                            {
                                                vec.push_back(val);
                                            }
                                        }

                                        combine_crossings = true;
                                    }
                                }
                                if (!combine_crossings)
                                {
                                    for (const auto& cross_it : crossing_fo_iterator_same_penalty)
                                    {
                                        // e2 container contains the same source
                                        auto& prev_cross = result.crossings[cross_it];
                                        assert(prev_cross.e2[0].source == e.source);
                                        // check if e1 is equal
                                        for (const auto& penalty_edges : prev_cross.e1)
                                        {
                                            if (penalty_edges == prev_edge)
                                            {
                                                // find the minimum of prev_cross.e1.target
                                                auto min_target_edge =
                                                    std::min_element(prev_cross.e1.begin(), prev_cross.e1.end(),
                                                                     [this](const edge& a, const edge& b)
                                                                     {
                                                                         return fanout_ntk.rank_position(a.target) <
                                                                                fanout_ntk.rank_position(b.target);
                                                                     });
                                                auto min_target = (*min_target_edge).target;

                                                if (fanout_ntk.rank_position(e.target) <=
                                                    fanout_ntk.rank_position(min_target))
                                                {
                                                    if (prev_cross.e2.back() != e)
                                                    {
                                                        prev_cross.e2.push_back(e);
                                                        for (const auto& e2 : prev_cross.e2)
                                                        {
                                                            const auto val = prev_cross.e2.size();
                                                            auto&      vec = multi_signal[e2];

                                                            const auto target = val - 1;
                                                            if (auto it_t = std::find(vec.begin(), vec.end(), target);
                                                                it_t != vec.end())
                                                            {
                                                                ++(*it_t);
                                                            }
                                                            else
                                                            {
                                                                vec.push_back(val);
                                                            }
                                                        }
                                                    }
                                                    combine_crossings = true;
                                                    break;
                                                }
                                            }
                                        }
                                    }
                                    if (!combine_crossings)
                                    {
                                        crossing_fo_iterator_same_edge.push_back(
                                            static_cast<uint32_t>(result.crossings.size()));

                                        crossing_fo_iterator_same_penalty.push_back(
                                            static_cast<uint32_t>(result.crossings.size()));

                                        result.crossings.emplace_back(prev_edge, e, 0);
                                        affected_edges.push_back(e);

                                        /*if (crossing_fo_iterator_same_edge.size() == 1)
                                        {

                                        }
                                        else
                                        {
                                            std::sort(
                                                crossing_fo_iterator_same_edge.begin(),
                                                crossing_fo_iterator_same_edge.end(),
                                                [this, &result](const auto& a,
                                                                const auto& b)
                                                {
                                                    return fanout_ntk.rank_position(
                                                               result.crossings[a]
                                                                   .e1[0]
                                                                   .source) >
                                                           fanout_ntk.rank_position(
                                                               result.crossings[b]
                                                                   .e1[0]
                                                                   .source);
                                                });
                                            uint64_t local_level = 0;
                                            for (uint64_t ll = 0u;
                                                 ll < crossing_fo_iterator_same_edge.size();
                                                 ++ll)
                                            {
                                                auto& c =
                                                    result.crossings
                                                        [crossing_fo_iterator_same_edge
                                                             [ll]];
                                                local_level = std::max(local_level, ll);
                                                local_level =
                                                    std::max(c.level, local_level);
                                                if (local_level > c.level)
                                                {
                                                    // update all crossings with the same
                                                    // penalty
                                                }
                                                c.level = local_level;
                                            }
                                        }*/

                                        total_crossings_fo++;

                                        // per-rank accounting + threshold
                                        ++rank_crossings;
                                        if (rank_crossings > cross_limit)
                                        {
                                            return;
                                        }
                                    }
                                }

                                affected_edges.push_back(prev_edge);
                            }

                            // optional micro-optimization: if we've exceeded already, stop
                            // inner loops
                            if (rank_crossings > cross_limit)
                            {
                                break;
                            }
                        }

                        if (rank_crossings > cross_limit)
                        {
                            break;
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

            if (rank_crossings > cross_limit)
            {
                // error message
                std::cerr << rank_crossings << " crossings detected in rank " << r
                          << ". Aborting crossing gate planarization.\n";

                return false;
            }

            for (auto const& e : stage_edges)
            {
                if (std::find(affected_edges.begin(), affected_edges.end(), e) == affected_edges.end())
                {
                    result.unaffected.push_back(e);
                }
            }

            crossing_ctn_fo.push_back(std::move(result));
        }

        return true;
    }

    bool ncross_extended()
    {
        total_crossings = 0;
        crossing_ctn.clear();

        for (uint32_t r = 0u; r < fanout_ntk.depth(); ++r)
        {
            uint32_t cross_limit = 1000u;
            // per-rank crossing limit check
            uint32_t rank_crossings = 0u;

            uint64_t next_width = fanout_ntk.rank_width(r + 1);

            std::vector<std::deque<std::pair<edge, uint64_t>>> penalty(next_width + 1);
            uint64_t                                           max_pos = 0;

            stage_result      result{};
            std::vector<edge> affected_edges;
            std::vector<edge> stage_edges;

            fanout_ntk.foreach_node_in_rank(r,
                                            [this, &penalty, &max_pos, &result, &affected_edges, &stage_edges,
                                             &rank_crossings, &cross_limit](auto const& n)
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

                                                    for (auto k = static_cast<uint64_t>(max_pos);
                                                         k >= static_cast<uint64_t>(pos + 1); --k)
                                                    {
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

                                                            // per-rank accounting + threshold
                                                            ++rank_crossings;
                                                            if (rank_crossings > cross_limit)
                                                            {
                                                                return;
                                                            }

                                                            local_lvl++;
                                                        }

                                                        // optional micro-optimization: if we've exceeded already, stop
                                                        // inner loops
                                                        if (rank_crossings > cross_limit)
                                                        {
                                                            break;
                                                        }
                                                    }

                                                    if (rank_crossings > cross_limit)
                                                    {
                                                        break;
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

            if (rank_crossings > cross_limit)
            {
                // error message
                std::cerr << rank_crossings << " crossings detected in rank " << r
                          << ". Aborting crossing gate planarization.\n";

                return false;
            }

            for (auto const& e : stage_edges)
            {
                if (std::find(affected_edges.begin(), affected_edges.end(), e) == affected_edges.end())
                {
                    result.unaffected.push_back(e);
                }
            }

            crossing_ctn.push_back(std::move(result));
        }

        return true;
    }

    Ntk run()
    {
        auto  init     = initialize_copy_virtual_pi_network<Ntk>(ntk);
        auto& ntk_dest = init.first;
        auto& old2new  = init.second;

        // ncross_fo();

        // crossing information
        if (!ncross_extended())
        {
            return ntk;  // abort planarization for large number of crossings
        }
        if (ps.verbose)
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

            auto     last           = initial;
            uint32_t crossing_depth = ps.xor_gates ? 4u : 14u;

            for (uint32_t j = 0; j < crossing_depth; ++j)
            {
                last = ntk_dest.create_buf(last);
            }

            // update mapping
            edge_to_node[edge] = last;

            return last;
        };

        auto simple_buf_chain = [&](mockturtle::signal<Ntk> s, uint32_t n)
        {
            for (uint32_t i = 0; i < n; ++i) s = ntk_dest.create_buf(s);
            return s;
        };

        auto buffered_xor_gate = [&](mockturtle::signal<Ntk> a, mockturtle::signal<Ntk> b)
        {
            auto fo1 = ntk_dest.create_buf(a);
            auto fo2 = ntk_dest.create_buf(b);
            // small buffer chain on 'a' for the first partial path
            auto a_buf2 = simple_buf_chain(fo1, 3);

            // core term: NOT(a AND b)
            auto core = ntk_dest.create_and(fo1, fo2);
            core      = ntk_dest.create_not(core);
            core      = ntk_dest.create_buf(core);

            // first partial: a_buf2 AND core
            auto p_a = ntk_dest.create_and(a_buf2, core);

            // small buffer chain on 'b' for the second partial path
            auto b_buf2 = simple_buf_chain(fo2, 3);
            auto p_b    = ntk_dest.create_and(b_buf2, core);

            // final OR
            return ntk_dest.create_or(p_a, p_b);
        };

        auto xor_decomposition_gate = [&](mockturtle::signal<Ntk> a, mockturtle::signal<Ntk> b)
        {
            // core = NOT(a AND b)
            auto core = ntk_dest.create_and(a, b);
            core      = ntk_dest.create_not(core);
            core      = ntk_dest.create_buf(core);

            // partials: a AND core, b AND core
            auto p_a = ntk_dest.create_and(a, core);
            auto p_b = ntk_dest.create_and(b, core);

            // OR of partials
            return ntk_dest.create_or(p_a, p_b);
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

            sig1 = ntk_dest.create_buf(sig1);
            sig2 = ntk_dest.create_buf(sig2);

            mockturtle::signal<Ntk> sig3{};

            if (ps.xor_gates)
            {
                if (ps.buffer)
                {
                    sig3 = simple_buf_chain(sig1, 2);
                }

                auto c0 = ntk_dest.create_xor(sig1, sig2);
                c0      = ntk_dest.create_buf(c0);

                mockturtle::signal<Ntk> c1{};
                mockturtle::signal<Ntk> c2{};

                if (sig3)
                {
                    sig2 = simple_buf_chain(sig2, 2);
                    c1   = ntk_dest.create_xor(sig3, c0);
                    c2   = ntk_dest.create_xor(c0, sig2);
                }
                else
                {
                    c1 = ntk_dest.create_xor(sig1, c0);
                    c2 = ntk_dest.create_xor(c0, sig2);
                }

                // update mapping for future chaining
                edge_to_node[e1] = c2;
                edge_to_node[e2] = c1;
            }
            else
            {
                mockturtle::signal<Ntk> c0{};
                mockturtle::signal<Ntk> ca1{};
                mockturtle::signal<Ntk> ca2{};

                mockturtle::signal<Ntk> fo0{};
                mockturtle::signal<Ntk> fo1{};
                mockturtle::signal<Ntk> fo2{};
                mockturtle::signal<Ntk> fo3{};

                if (ps.buffer)
                {
                    sig3 = simple_buf_chain(sig1, 7);
                    c0   = buffered_xor_gate(sig1, sig2);
                }
                else
                {
                    fo0 = ntk_dest.create_buf(sig1);
                    // core = NOT(a AND b)
                    auto core = ntk_dest.create_and(sig1, sig2);
                    sig2      = ntk_dest.create_buf(sig2);
                    fo1       = ntk_dest.create_buf(fo0);
                    core      = ntk_dest.create_not(core);
                    core      = ntk_dest.create_buf(core);
                    fo2       = ntk_dest.create_buf(sig2);

                    // partials: a AND core, b AND core
                    auto p_a = ntk_dest.create_and(fo0, core);
                    auto p_b = ntk_dest.create_and(sig2, core);

                    // OR of partials
                    c0  = ntk_dest.create_or(p_a, p_b);
                    c0  = ntk_dest.create_buf(c0);
                    fo3 = ntk_dest.create_buf(c0);
                    c0  = ntk_dest.create_buf(c0);
                }

                mockturtle::signal<Ntk> c1{};
                mockturtle::signal<Ntk> c2{};

                if (ps.buffer)
                {
                    c0   = ntk_dest.create_buf(c0);
                    sig2 = simple_buf_chain(sig2, 7);
                    c1   = buffered_xor_gate(sig3, c0);
                    c2   = buffered_xor_gate(c0, sig2);
                }
                else
                {
                    c1 = xor_decomposition_gate(fo1, fo3);
                    c2 = xor_decomposition_gate(c0, fo2);
                }

                // update mapping for future chaining
                edge_to_node[e1] = c2;
                edge_to_node[e2] = c1;
            }
        };

        /*auto place_crossing_fo = [&](auto const& item)
        {
            auto const& e1 = item.e1;
            auto const& e2 = item.e2;

            // all edges in e1 and e2 have the same source, so we can just check one for each
            auto it1 = edge_to_node.find(e1.back());
            auto it2 = edge_to_node.find(e2.back());

            typename Ntk::node child1 = it1 != edge_to_node.end() ? it1->second : old2new[e1.back().source];

            typename Ntk::node child2 = it2 != edge_to_node.end() ? it2->second : old2new[e2.back().source];

            auto sig1 = ntk_dest.make_signal(child1);
            auto sig2 = ntk_dest.make_signal(child2);

            sig1 = ntk_dest.create_buf(sig1);
            sig2 = ntk_dest.create_buf(sig2);

            mockturtle::signal<Ntk> sig3{};

            if (ps.xor_gates)
            {
                if (ps.buffer)
                {
                    sig3 = simple_buf_chain(sig1, 2);
                }

                auto c0 = ntk_dest.create_xor(sig1, sig2);
                c0      = ntk_dest.create_buf(c0);

                mockturtle::signal<Ntk> c1{};
                mockturtle::signal<Ntk> c2{};

                if (sig3)
                {
                    sig2 = simple_buf_chain(sig2, 2);
                    c1   = ntk_dest.create_xor(sig3, c0);
                    c2   = ntk_dest.create_xor(c0, sig2);
                }
                else
                {
                    c1 = ntk_dest.create_xor(sig1, c0);
                    c2 = ntk_dest.create_xor(c0, sig2);
                }

                // update mapping for future chaining
                for (const auto& e : e1)
                {
                    edge_to_node[e] = c2;
                }
                for (const auto& e : e2)
                {
                    edge_to_node[e] = c1;
                }
            }
            else
            {
                mockturtle::signal<Ntk> c0{};
                mockturtle::signal<Ntk> ca1{};
                mockturtle::signal<Ntk> ca2{};

                mockturtle::signal<Ntk> fo0{};
                mockturtle::signal<Ntk> fo1{};
                mockturtle::signal<Ntk> fo2{};
                mockturtle::signal<Ntk> fo3{};

                if (ps.buffer)
                {
                    sig3 = simple_buf_chain(sig1, 7);
                    c0   = buffered_xor_gate(sig1, sig2);
                }
                else
                {
                    fo0 = ntk_dest.create_buf(sig1);
                    // core = NOT(a AND b)
                    auto core = ntk_dest.create_and(sig1, sig2);
                    sig2      = ntk_dest.create_buf(sig2);
                    fo1       = ntk_dest.create_buf(fo0);
                    core      = ntk_dest.create_not(core);
                    core      = ntk_dest.create_buf(core);
                    fo2       = ntk_dest.create_buf(sig2);

                    // partials: a AND core, b AND core
                    auto p_a = ntk_dest.create_and(fo0, core);
                    auto p_b = ntk_dest.create_and(sig2, core);

                    // OR of partials
                    c0  = ntk_dest.create_or(p_a, p_b);
                    c0  = ntk_dest.create_buf(c0);
                    fo3 = ntk_dest.create_buf(c0);
                    c0  = ntk_dest.create_buf(c0);
                }

                mockturtle::signal<Ntk> c1{};
                mockturtle::signal<Ntk> c2{};

                if (ps.buffer)
                {
                    c0   = ntk_dest.create_buf(c0);
                    sig2 = simple_buf_chain(sig2, 7);
                    c1   = buffered_xor_gate(sig3, c0);
                    c2   = buffered_xor_gate(c0, sig2);
                }
                else
                {
                    c1 = xor_decomposition_gate(fo1, fo3);
                    c2 = xor_decomposition_gate(c0, fo2);
                }

                // update mapping for future chaining
                for (const auto& e : e1)
                {
                    edge_to_node[e] = c2;
                }
                for (const auto& e : e2)
                {
                    edge_to_node[e] = c1;
                }
            }
        };

        auto swap_adjacent_blocks = [](auto& v, std::size_t it, std::size_t a, std::size_t b)
        {
            // preconditions (you can replace with asserts if you prefer)
            if (a == 0 || b == 0)
                return;
            if (it > v.size())
                return;
            if (it + a + b > v.size())
                return;

            // [it .. it+a) [it+a .. it+a+b)  ->  [it+a .. it+a+b) [it .. it+a)
            std::rotate(v.begin() + it, v.begin() + it + a, v.begin() + it + a + b);
        };*/

        // === process per rank (edges are between rank r-1 and r) ===
        for (uint32_t r = 1u; r < fanout_ntk.depth() + 1; ++r)
        {
            // crossings between rank r-1 and r
            auto ordered   = crossing_ctn[r - 1].crossings;  // copy so we can sort

            // sort by level
            std::sort(ordered.begin(), ordered.end(), [](auto const& a, auto const& b) { return a.level < b.level; });

            uint32_t cross_it = 0;
            auto&    edges    = crossing_ctn[r - 1].edges;

            while (!ordered.empty())
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
                    else if (ps.buffer)
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

            /*uint32_t cross_it_fo = 0;
            auto&    edges_fo    = crossing_ctn_fo[r - 1].edges;
            auto crossings = crossing_ctn_fo[r - 1].crossings;
            std::unordered_set<edge, edge_hash> used_edges;
            auto check_front_equals = [&](const std::vector<edge>& block, std::size_t expected) -> bool
            {
                for (const auto& e : block)
                {
                    // here check if the edge was already used
                    if (used_edges.contains(e))
                    {
                        return false;
                    }

                    /*auto it = multi_signal.find(e);
                    if (it == multi_signal.end())
                    {
                        continue; // key not present -> ok
                    }

                    const auto& v = it->second;
                    if (v.empty())
                    {
                        continue; // present but empty -> ok (already used up)
                    }

                    if (v.front() != expected)
                    {
                        return false; // mismatch -> fail
                    }#1#
                }
                return true;
            };

            auto consume_front_equals = [&](const std::vector<edge>& block, std::size_t expected) -> void
            {
                for (const auto& e : block)
                {
                    auto it = multi_signal.find(e);
                    if (it == multi_signal.end())
                    {
                        continue;
                    }

                    auto& v = it->second;
                    if (!v.empty() && v.front() == expected)
                    {
                        v.erase(v.begin()); // consume
                    }
                }
            };

            auto claim_edges = [&](const std::vector<edge>& block) -> void
            {
                for (const auto& e : block)
                {
                    used_edges.insert(e);
                }
            };

            auto find_adjacent_pos = [&claim_edges, &check_front_equals](const std::vector<edge>& edges_fo, const std::vector<edge>& e1,
                                                    const std::vector<edge>& e2) -> std::optional<std::size_t>
            {
                if (e1.empty() || e2.empty())
                {
                    return std::nullopt;
                }

                // check only (no consumption)
                // in check_front_equals i can check whether my edges are already used
                if (!check_front_equals(e1, e1.size()) || !check_front_equals(e2, e2.size()))
                {
                    return std::nullopt;
                }

                const std::size_t m = e1.size();
                const std::size_t n = e2.size();

                if (m + n > edges_fo.size())
                {
                    return std::nullopt;
                }

                std::vector<edge> pattern;
                pattern.reserve(m + n);
                pattern.insert(pattern.end(), e1.begin(), e1.end());
                pattern.insert(pattern.end(), e2.begin(), e2.end());

                auto it = std::ranges::search(edges_fo, pattern).begin();
                if (it == edges_fo.end())
                {
                    return std::nullopt;
                }

                claim_edges(e1);
                claim_edges(e2);

                return static_cast<std::size_t>(std::distance(edges_fo.begin(), it));
            };

            auto swap_adjacent_at = [](std::vector<edge>& edges_fo, std::size_t pos, std::size_t m,
                                       std::size_t n) -> void
            { std::rotate(edges_fo.begin() + pos, edges_fo.begin() + pos + m, edges_fo.begin() + pos + m + n); };

            struct pending_crossing
            {
                std::size_t crossing_index;  // index in crossings vector (stable, because we never modify crossings)
                std::size_t pos;             // start position in fanout_fo at scan time
                std::size_t m;               // e1.size()
                std::size_t n;               // e2.size()
            };

            // Active index list (crossings is never modified)
            std::vector<std::size_t> active(crossings.size());
            std::iota(active.begin(), active.end(), 0);

            while (!active.empty())
            {
                used_edges.clear();
                used_edges.reserve(active.size() * 2); // optional

                std::vector<pending_crossing> pending;
                pending.reserve(active.size());

                // Phase A: collect candidates
                for (auto ci : std::ranges::reverse_view(active))
                {
                    const auto& e1 = crossings[ci].e1;
                    const auto& e2 = crossings[ci].e2;

                    // here i call find_adjacent_pos
                    if (auto pos = find_adjacent_pos(edges_fo, e1, e2))
                    {
                        pending.push_back(pending_crossing{ci, *pos, e1.size(), e2.size()});
                    }
                }

                if (pending.empty())
                {
                    if (!active.empty())
                    {

                        /*throw std::runtime_error("Crossings could not be resolved in level " + std::to_string(r) +
                                                 ". Aborting.");#1#
                        std::cout << "Not all crossings used at lvl : " << r << "\n";
                    }
                    break;
                }

                // Phase B: process pending crossings here (if needed)


                /*std::ranges::sort(pending, {}, &pending_crossing::pos);

                for (std::size_t i = 1; i < pending.size(); ++i)
                {
                    const auto& a = pending[i - 1];
                    const auto& b = pending[i];
                    if (a.pos + a.m + a.n > b.pos)
                    {
                        throw std::runtime_error("pending swaps overlap; positions are not independent");
                    }
                }#1#

                std::vector<std::size_t> rm;
                rm.reserve(pending.size());
                // Phase C: apply swaps using stored positions
                for (const auto& p : pending)
                {
                    // place crossing
                    place_crossing_fo(crossings[p.crossing_index]);

                    // clean up multi_signals
                    const auto& e1 = crossings[p.crossing_index].e1;
                    const auto& e2 = crossings[p.crossing_index].e2;
                    // consume_front_equals(e1, e1.size());
                    // consume_front_equals(e2, e2.size());

                    // swap edges
                    swap_adjacent_at(edges_fo, p.pos, p.m, p.n);

                    rm.push_back(p.crossing_index);

                    // update active indices
                    /*if (auto it = std::find(active.begin(), active.end(), p.crossing_index); it != active.end())
                    {
                        *it = active.back();
                        active.pop_back();
                    }#1#
                }
                std::ranges::sort(rm);
                std::size_t out = 0;
                std::size_t j   = 0;

                for (std::size_t i = 0; i < active.size(); ++i)
                {
                    const auto a = active[i];

                    while (j < rm.size() && rm[j] < a)
                    {
                        ++j;
                    }

                    if (j < rm.size() && rm[j] == a)
                    {
                        continue;  // remove
                    }

                    active[out++] = a;  // keep (order preserved)
                }

                active.resize(out);
            }*/

                /*for (size_t i = 0; i < edges_fo.size(); ++i)
                {
                    auto e                     = edges_fo[i];
                    auto next_crossing_edge_fo = ordered_fo[cross_it_fo].e1.back();

                    // _fo version
                    if (next_crossing_edge_fo == e)
                    {
                        assert(i + ordered_fo[cross_it_fo].e1.size() + ordered_fo[cross_it_fo].e2.size() - 1 <
                               edges_fo.size());
                        place_crossing_fo(ordered_fo[cross_it_fo]);

                        // swap all edges contributing to the crossing
                        swap_adjacent_blocks(edges_fo, i, ordered_fo[cross_it_fo].e1.size(),
                                             ordered_fo[cross_it_fo].e2.size());

                        i += ordered_fo[cross_it_fo].e1.size() + ordered_fo[cross_it_fo].e2.size() - 1;
                    }
                    else if (ps.buffer)
                    {
                        create_buffer_chain(e);
                    }
                }

                // if no crossings placed during loop --> layer is stable, stop
                if (++cross_it_fo >= ordered_fo.size())
                {
                    std::cerr << "No crossing was placed in this round; Abort\n";
                }*/

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

        const auto pi_ranks = ntk.get_ranks(0);
        ntk_dest.set_ranks(0, pi_ranks);

        return ntk_dest;
    }

  private:
    Ntk                                ntk{};
    mockturtle::fanout_view<Ntk>       fanout_ntk{};
    crossing_gate_planarization_params ps{};
    uint64_t                           total_crossings{0};
    std::vector<stage_result>          crossing_ctn{};

    uint64_t                                                   total_crossings_fo{0};
    std::vector<stage_result_fo>                               crossing_ctn_fo{};
    std::unordered_map<edge, std::vector<uint64_t>, edge_hash> multi_signal{};
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

    debug::write_dot_network(result, "result");

    // check for planarity
    /*if (ps.buffer)
    {
        mincross_stats  st_min{};
        mincross_params p_min{};
        p_min.optimize = false;

        auto ntk_min = mincross(result, p_min, &st_min);  // counts crossings
        if (st_min.num_crossings != 0)
        {
            throw std::runtime_error("Planarization failed: resulting network is not planar");
        }
    }*/

    return result;
}

}  // namespace fiction
#endif  // FICTION_CROSSING_GATE_PLANARIZATION_FO_HPP
