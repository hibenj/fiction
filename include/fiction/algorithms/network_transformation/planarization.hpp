//
// Created by benjamin on 09.12.25.
//

#ifndef FICTION_PLANARIZATION_HPP
#define FICTION_PLANARIZATION_HPP

#include "fiction/algorithms/graph/mincross.hpp"
#include "fiction/algorithms/network_transformation/network_balancing.hpp"
#include "fiction/algorithms/network_transformation/node_duplication_planarization.hpp"
#include "fiction/networks/virtual_pi_network.hpp"

#include <mockturtle/traits.hpp>
#include <mockturtle/utils/node_map.hpp>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

namespace fiction
{

/**
 * Parameters for the node duplication algorithm.
 */
struct planarization_params
{
    /**
     * Controls how output nodes are ordered before starting the algorithm.
     */
    enum class output_order : uint8_t
    {
        /**
         * Keep the PO order from the input network.
         */
        KEEP_PO_ORDER,
        /**
         * Randomize the PO order.
         */
        RANDOM_PO_ORDER
    };
    /**
     * The output order used. Defaults to KEEP_PO_ORDER.
     */
    output_order po_order = output_order::KEEP_PO_ORDER;
    /**
     * Whether the otput network should be buffered.
     */
    bool buffer = true;
    /**
     * Whether xor gates are allowed in the network.
     */
    bool xor_gates = false;
};

namespace detail
{

template <typename Ntk, typename NtkDest>
std::vector<mockturtle::signal<NtkDest>>
gather_original_fanin_signals(const Ntk& ntk, NtkDest& ntk_dest_v, const mockturtle::node<Ntk> n,
                              const mockturtle::node_map<std::vector<mockturtle::signal<NtkDest>>, Ntk>& old2new_v,
                              const std::vector<mockturtle::node<NtkDest>>&                              lvl)
{
    std::vector<mockturtle::signal<NtkDest>> children{};

    ntk.foreach_fanin(n,
                      [&ntk, &ntk_dest_v, &old2new_v, &children](const auto& f)
                      {
                          const auto fn = ntk.get_node(f);
                          assert(old2new_v[fn].size() == 1);
                          const auto tgt_signal = old2new_v[fn][0];

                          children.emplace_back(ntk.is_complemented(f) ? ntk_dest_v.create_not(tgt_signal) :
                                                                         tgt_signal);
                      });

    return children;
};

/**
 * Constructs a planar `virtual_pi_network` based on duplicated nodes derived from the source network.
 *
 * The input `ntk_lvls` contains per-level vectors of original node ranks in the source network. For each level, this
 * function creates corresponding nodes (including duplicates) in a new `virtual_pi_network` and restores their fanin
 * relations using the `gather_fanin_signals` helper function.
 *
 * For duplicated PIs (Primary Inputs), virtual PIs are created, and the original PI is stored in a mapping structure.
 * The auxiliary function `gather_fanin_signals` collects fanin data for each node and matches it to its corresponding
 * nodes in the `virtual_pi_network`.
 *
 * Example: For a level {2, 3, 2, 4, 2}, new nodes are created for each duplicated occurrence (e.g., node 2) and stored
 * in the `old2new_v` node map. This map is then used by `gather_fanin_signals` to correctly establish fanin
 * relationships between newly created nodes.
 *
 * @tparam Ntk Network type.
 * @param ntk Source network used to construct the `virtual_pi_network`.
 * @param ntk_lvls Per-level vectors of original node ranks in the source network used to derive node duplications.
 * @param ntk_lvls_new Per-level vectors of newly created nodes' ranks in the constructed `virtual_pi_network`.
 * @return The constructed planar `virtual_pi_network` containing duplicated nodes with restored fanin and fanout
 * relations.
 */
template <typename Ntk>
virtual_pi_network<Ntk> create_virtual_pi_ntk_from_duplicated_nodes_with_keep_original_levels(
    const Ntk& ntk, const std::vector<std::vector<mockturtle::node<Ntk>>>& ntk_lvls,
    std::vector<std::vector<mockturtle::node<virtual_pi_network<Ntk>>>>& ntk_lvls_new, uint32_t cross_lvl)
{
    static_assert(mockturtle::has_create_node_v<virtual_pi_network<Ntk>>, "virtual_pi_network<Ntk> lacks create_node");
    static_assert(mockturtle::has_get_node_v<virtual_pi_network<Ntk>>, "virtual_pi_network<Ntk> lacks get_node");
    static_assert(mockturtle::has_fanout_size_v<virtual_pi_network<Ntk>>, "virtual_pi_network<Ntk> lacks fanout_size");
    static_assert(mockturtle::has_create_po_v<virtual_pi_network<Ntk>>, "virtual_pi_network<Ntk> lacks create_po");
    static_assert(mockturtle::has_create_not_v<virtual_pi_network<Ntk>>, "virtual_pi_network<Ntk> lacks create_not");

    std::unordered_map<mockturtle::node<Ntk>, bool> node_status;
    ntk_lvls_new.resize(ntk_lvls.size());

    auto  init_v     = initialize_copy_network_duplicates(ntk);
    auto& ntk_dest_v = init_v.first;
    auto& old2new_v  = init_v.second;

    for (auto i = ntk_lvls.size(); i-- > 0;)
    {
        // The index of the node in the current node level.
        std::size_t node_index = 0;

        // The current node level with duplicated nodes.
        // Example vector: {3, 2, 3}
        const auto& lvl = ntk_lvls[i];

        // The current node level in the new network, where duplicated nodes are created as new nodes.
        // Example vector {3, 2, 4}
        auto& lvl_new = ntk_lvls_new[i];

        // Create a node in the new network for each node contained in 'lvl'.
        for (const auto& nd : lvl)
        {
            // If the node is a PI create virtual PIs for duplicates.
            if (ntk.is_pi(nd))
            {
                if (node_status[nd])
                {
                    const auto new_sig = ntk_dest_v.create_virtual_pi(nd);
                    lvl_new.push_back(ntk_dest_v.get_node(new_sig));
                    old2new_v[nd].push_back(new_sig);
                }
                else
                {
                    const auto& sigs = old2new_v[nd];
                    assert(!sigs.empty());
                    lvl_new.push_back(ntk_dest_v.get_node(sigs.front()));
                    node_status[nd] = true;
                }
            }
            else
            {
                assert(i + 1 < ntk_lvls_new.size() && "Next level does not exist");

                auto children = gather_fanin_signals(ntk, ntk_dest_v, nd, old2new_v, ntk_lvls_new[i + 1], node_index);

                if (children.size() != ntk.fanin_size(nd))
                {
                    children = gather_original_fanin_signals(ntk, ntk_dest_v, nd, old2new_v, ntk_lvls_new[i + 1]);
                }

                // Ensure child count matches function arity (including 0-fanin constants)
                /*assert(children.size() == ntk.fanin_size(nd) &&
                       "Mismatch between gathered children and node fanin count");*/

                const auto new_sig = ntk_dest_v.create_node(children, ntk.node_function(nd));
                lvl_new.push_back(ntk_dest_v.get_node(new_sig));
                old2new_v[nd].push_back(new_sig);
            }
        }
    }

    ntk.foreach_po(
        [&ntk, &ntk_dest_v, &old2new_v](const auto& po)
        {
            const auto tgt_signal_v = old2new_v[ntk.get_node(po)];

            // POs are not duplicated as the algorithm starts at POs and duplicates other nodes based on their order
            assert(tgt_signal_v.size() == 1 && "Multiple nodes mapped to PO");

            const auto tgt_signal = tgt_signal_v[0];

            const auto tgt_po = ntk.is_complemented(po) ? ntk_dest_v.create_not(tgt_signal) : tgt_signal;

            ntk_dest_v.create_po(tgt_po);
        });

    return ntk_dest_v;
}

template <typename Ntk>
class planarization_impl
{
  public:
    [[maybe_unused]] planarization_impl(const Ntk& src, const planarization_params& p) :
            ntk(src),
            fanout_ntk(src),
            ps{p}
    {}

    /**
     * A "slice" describes one vertical layer in the H-graph. It is created by adding all possible combinations of a
     * `node_pair` to the H-graph of the level. These combinations are formed by selecting pairs of nodes from the
     * fan-ins of the input node:
     * - If the input node has only one fan-in, it is treated as a single combination.
     * - If the input node has two fan-ins, there are two possible combinations.
     *
     * Each `node_pair` consists of a first and second element. The objective is to find an ordering of node pairs that
     * maximizes the instances where the first element of a node_pair matches the second element of the preceding
     * node_pair. This ordering is given as a linked list.
     *
     * This function computes the optimal ordering by calculating delays as follows:
     * - All combinations of node pairs are iteratively added to a linked list.
     * - For each combination, the first element of the current node_pair is compared with the last element of the
     * preceding node_pairs.
     * - If a connection exists between two node_pairs, the delay increases by 1; otherwise, it increases by 2. The
     * default delay for the first node is 1.
     * - If a node_pair lacks a connection, and its updated delay (increased by 2) is less than the existing delay, the
     * node_pair's delay is updated accordingly.
     *
     * Processed node_pairs are stored in the `lvl_pairs` member for subsequent delay calculations.
     *
     * @param nd Node in the H-graph.
     */
    void compute_slice_delays(const mockturtle::node<Ntk>& nd)
    {
        // Pis need to be propagated into the next level, since they have to be connected without crossings
        if (ntk.is_pi(nd))
        {
            fis.push_back(nd);
        }

        // Respect the rank order. If two combinations have the same delay and have no seen advantage then the one from
        // the original ranking is used, since it is inserted and not overwritten afterward.
        ntk.foreach_fanin(nd,
                          [&](auto fi)
                          {
                              if (!ntk.is_constant(fi))
                              {
                                  auto n = ntk.get_node(fi);

                                  auto it =
                                      std::lower_bound(fis.begin(), fis.end(), n, [&](auto const& a, auto const& b)
                                                       { return ntk.rank_position(a) < ntk.rank_position(b); });

                                  fis.insert(it, n);
                              }
                          });

        assert(!fis.empty() && "There has to be at least one node in this level");

        // Compute the combinations in one slice
        auto combinations = calculate_pairs<Ntk>(fis);
        assert(!combinations.empty() && "Combinations are empty. There might be a dangling node");

        if (!lvl_pairs.empty())
        {
            std::vector<hgraph_node<Ntk>>* combinations_last = &lvl_pairs.back();

            for (std::size_t cur_idx = 0; cur_idx < combinations.size(); ++cur_idx)
            {
                auto& node_pair_cur = combinations[cur_idx];

                for (std::size_t last_idx = 0; last_idx < combinations_last->size(); ++last_idx)
                {
                    auto& node_pair_last = (*combinations_last)[last_idx];

                    // If there is a connection between the two node pairs the delay is calculated like this
                    if ((node_pair_cur.outer_fanins.first == node_pair_last.outer_fanins.second &&
                         node_pair_last.delay + 1 < node_pair_cur.delay))
                    {
                        node_pair_cur.fanin_it = last_idx;
                        node_pair_cur.delay    = node_pair_last.delay + 1;
                    }
                    // If there is no connection between the two node pairs the delay is calculated like this
                    else if (node_pair_last.delay + 2 < node_pair_cur.delay)
                    {
                        node_pair_cur.fanin_it = last_idx;
                        node_pair_cur.delay    = node_pair_last.delay + 2;
                    }
                    else if (node_pair_last.delay + 2 == node_pair_cur.delay)
                    {
                        // This solves equal path delays, if they are connected in the next layer via a fanout
                        const auto fc0 = fanins(ntk, node_pair_cur.outer_fanins.first);
                        if (node_pair_last.fanin_it < combinations_last->size())
                        {
                            const auto fc1 = fanins(ntk, node_pair_last.outer_fanins.second);

                            for (const auto f0 : fc0.fanin_nodes)
                            {
                                for (const auto f1 : fc1.fanin_nodes)
                                {
                                    if (f0 == f1)
                                    {
                                        node_pair_cur.fanin_it = last_idx;
                                        goto next_combination;
                                    }
                                }
                            }
                        }
                    }
                }
            next_combination:;
            }
        }
        else
        {
            // The delay for the first node in the level is set to 1
            for (auto& node_pair : combinations)
            {
                node_pair.delay = 1;
            }
        }

        lvl_pairs.push_back(combinations);
    }

    /**
     * Inserts a node into a vector if it is unique.
     *
     * This function inserts a node into a vector only if the vector is empty or the node is not equal to the first
     * element of the vector. If the vector is not empty and the node is equal to the first element, insertion depends
     * on the `saturated_fanout_flag` and the node's `position`: when `position == 0`, a repeated insertion attempt will
     * succeed only if the node was previously skipped (indicated by `saturated_fanout_flag == 1`); otherwise, the flag
     * is set to 1 and the node is skipped for this call. No exception is thrown during this process.
     *
     * @param node The node to be inserted.
     * @param vec The vector to insert the node into.
     * @param saturated_fanout_flag A state flag toggled when consecutive duplicate insertions occur. Set to 1 when a
     * node is skipped and reset to 0 when a node is successfully inserted.
     * @param position The position of the node (0 indicates a terminal node; controls duplicate insertion behavior).
     */
    void insert_if_not_first(const mockturtle::node<Ntk>& node, std::vector<mockturtle::node<Ntk>>& vec,
                             int& saturated_fanout_flag, const int position)
    {
        if (vec.empty() || vec.front() != node)
        {
            vec.insert(vec.begin(), node);
            saturated_fanout_flag = 0;
        }
        else if (position == 0)
        {
            if (saturated_fanout_flag == 1)
            {
                vec.insert(vec.begin(), node);
                saturated_fanout_flag = 0;
            }
            else
            {
                if (ntk.fanout_size(node) == 1)
                {
                    vec.insert(vec.begin(), node);
                }
                saturated_fanout_flag = 1;
            }
        }
    }

    /**
     * This function computes the order of nodes in the next level based on their delay in the H-graph of the level. It
     * selects the path with the least delay from the current level pairs and follows it via fanin relations. The nodes
     * are inserted into the next level vector in the order they are encountered.
     *
     * @return The order of nodes in `next_level`
     */
    std::vector<mockturtle::node<Ntk>> compute_node_order()
    {
        std::vector<mockturtle::node<Ntk>> next_level;
        int                                saturated_fanout_flag = 0;

        const auto& combinations = lvl_pairs.back();

        // Select the path with the least delay and follow it via fanin relations
        const auto minimum_it =
            std::min_element(combinations.cbegin(), combinations.cend(),
                             [](const hgraph_node<Ntk>& a, const hgraph_node<Ntk>& b) { return a.delay < b.delay; });

        if (minimum_it != combinations.cend())
        {
            const auto& min_combination = *minimum_it;

            // Insert the terminal node
            insert_if_not_first(min_combination.outer_fanins.second, next_level, saturated_fanout_flag, 0);

            // Insert middle_fanins
            for (const auto& node : min_combination.middle_fanins)
            {
                insert_if_not_first(node, next_level, saturated_fanout_flag, 1);
            }

            // Insert the first node
            insert_if_not_first(min_combination.outer_fanins.first, next_level, saturated_fanout_flag, 1);

            // Start with index instead of pointer
            std::size_t level    = lvl_pairs.size() - 1;
            std::size_t fanin_it = minimum_it->fanin_it;

            // Follow chain while index is valid
            while (level > 0 && fanin_it < lvl_pairs[level - 1].size())
            {
                const auto& fanin_combination = lvl_pairs[level - 1][fanin_it];

                // Insert the terminal node
                if (ntk.is_pi(fanin_combination.outer_fanins.second))
                {
                    saturated_fanout_flag = 1;
                }
                insert_if_not_first(fanin_combination.outer_fanins.second, next_level, saturated_fanout_flag, 0);

                // Insert middle_fanins
                for (const auto& node : fanin_combination.middle_fanins)
                {
                    insert_if_not_first(node, next_level, saturated_fanout_flag, 1);
                }

                // Insert the first node
                insert_if_not_first(fanin_combination.outer_fanins.first, next_level, saturated_fanout_flag, 1);

                // Move one level up
                --level;
                fanin_it = fanin_combination.fanin_it;
            }
        }

        return next_level;
    }

    /**
     * Checks if the given vector of nodes contains any non-primary inputs.
     *
     * @param v_next_level The vector of nodes to be checked.
     */
    [[nodiscard]] bool check_final_level(const std::vector<mockturtle::node<Ntk>>& v_next_level)
    {
        for (const auto& nd : v_next_level)
        {
            if (!ntk.is_pi(nd))
            {
                return false;
            }
        }
        return true;
    }

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

    struct stage_result_old
    {
        uint64_t                               max_level;
        std::vector<crossing_item>             crossings;
        std::vector<edge>                      unaffected;
        std::vector<edge>                      edges;
        std::map<uint64_t, uint64_t>           crossings_per_level;
        std::vector<std::pair<edge, uint64_t>> crossings_per_edge;
    };

    struct stage_result
    {
        uint64_t                               max_level;
        uint32_t                               n_crossings;
        std::vector<std::pair<edge, uint64_t>> crossings_per_edge;
    };

    /*void ncross_extended()
    {
        crossing_ctn.clear();

        for (uint32_t lvl = 0u; lvl < fanout_ntk.depth(); ++lvl)
        {
            uint64_t next_width = fanout_ntk.rank_width(lvl + 1);

            std::vector<std::deque<std::pair<edge, uint64_t>>> penalty(next_width + 1);
            uint64_t                                           max_pos = 0;

            stage_result      result{};
            std::vector<edge> affected_edges;
            std::vector<edge> stage_edges;

            // helper to increment per-edge crossing count stored in result.crossings_per_edge
            auto increment_count = [&result](const edge& ed)
            {
                for (auto& p : result.crossings_per_edge)
                {
                    if (p.first == ed)
                    {
                        ++p.second;
                        return;
                    }
                }
                result.crossings_per_edge.emplace_back(ed, 1);
            };

            fanout_ntk.foreach_node_in_rank(
                lvl,
                [this, &penalty, &max_pos, &result, &affected_edges, &stage_edges, &increment_count](auto const& n)
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

                                uint64_t level   = std::max(local_lvl, prev_lvl);
                                result.max_level = std::max(result.max_level, level);

                                result.crossings.emplace_back(prev_edge, e, level);
                                result.crossings_per_level[level]++;

                                // increment counts for both involved edges
                                increment_count(prev_edge);
                                increment_count(e);

                                affected_edges.push_back(e);
                                affected_edges.push_back(prev_edge);

                                if (prev_lvl > local_lvl)
                                {
                                    local_lvl = prev_lvl;
                                }

                                prev_lvl++;
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

            // ensure all stage edges have an entry in crossings_per_edge (unaffected ones get count 0)
            for (auto const& e : stage_edges)
            {
                bool found = false;
                for (auto const& p : result.crossings_per_edge)
                {
                    if (p.first == e)
                    {
                        found = true;
                        break;
                    }
                }
                if (!found)
                {
                    result.crossings_per_edge.emplace_back(e, 0);
                }
            }

            crossing_ctn.push_back(std::move(result));
        }
    }*/

    stage_result ncross_fanins(const std::vector<mockturtle::node<Ntk>>& level_v, const std::vector<mockturtle::node<Ntk>>& next_level_v, uint32_t lvl)
    {
        stage_result result{};

        // We index by *source* position in previous rank (r-1)
        uint64_t                                           prev_width = fanout_ntk.rank_width(lvl - 1);
        std::vector<std::deque<std::pair<edge, uint64_t>>> penalty(prev_width + 1);
        uint64_t                                           max_pos = 0;

        std::vector<edge> stage_edges;

        // helper to increment per-edge crossing count stored in result.crossings_per_edge
        auto increment_count = [&result](edge const& ed)
        {
            for (auto& p : result.crossings_per_edge)
            {
                if (p.first == ed)
                {
                    ++p.second;
                    result.max_level = std::max(result.max_level, static_cast<uint64_t>(p.second));
                    return;
                }
            }
            result.crossings_per_edge.emplace_back(ed, 1);
            result.max_level = std::max(result.max_level, static_cast<uint64_t>(1));
        };

        for (const auto& n : level_v)
        {
            // Collect incoming edges (fi -> n) with their source positions
            std::vector<std::pair<uint64_t, edge>> incoming;
            incoming.reserve(fanout_ntk.fanin_size(n));  // if available; otherwise remove

            fanout_ntk.foreach_fanin(n,
                                     [&](auto const& fi)
                                     {
                                         auto it_pos = std::find(next_level_v.begin(), next_level_v.end(), fi);
                                         assert(it_pos != next_level_v.end());
                                         auto pos = static_cast<uint64_t>(std::distance(next_level_v.begin(), it_pos));
                                         edge e{fi, n};

                                         incoming.emplace_back(pos, e);
                                     });

            // Detect crossings against previously inserted edges:
            // For each new edge with source position 'pos', all previously seen edges
            // with source position > pos will cross it (because targets are scanned L->R).
            for (auto const& [pos, e] : incoming)
            {
                // uint64_t local_lvl = 0;

                // iterate from current max_pos down to pos+1
                for (uint64_t k = max_pos; k >= pos + 1; --k)
                {
                    // newest edges first (matches your other routine)
                    for (auto it = penalty[k].begin(); it != penalty[k].end(); ++it)
                    {
                        auto const& prev_edge = it->first;
                        // auto&       prev_lvl  = it->second;

                        /*uint64_t level   = std::max(local_lvl, prev_lvl);
                        result.max_level = std::max(result.max_level, level);*/

                        result.n_crossings++;

                        // increment counts for both involved edges
                        increment_count(prev_edge);
                        increment_count(e);

                        /*if (prev_lvl > local_lvl)
                        {
                            local_lvl = prev_lvl;
                        }

                        ++prev_lvl;
                        ++local_lvl;*/
                    }
                }
            }

            // Insert these edges into penalty buckets by their source position
            for (auto const& [pos, e] : incoming)
            {
                max_pos = std::max(max_pos, pos);
                penalty[pos].push_front({e, 0});
                stage_edges.push_back(e);
            }
        }

        // ensure all stage edges have an entry in crossings_per_edge (unaffected ones get count 0)
        for (auto const& e : stage_edges)
        {
            bool found = false;
            for (auto const& p : result.crossings_per_edge)
            {
                if (p.first == e)
                {
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                result.crossings_per_edge.emplace_back(e, 0);
            }
        }

        return result;
    }

    void assign_duplicaton_costs()
    {
        ntk.foreach_node(
            [&](const auto n)
            {
                // duplication_cost[n] = ntk.is_fanout(n) ? 0u : 1u;
                duplication_cost[n] = 1u;
                ntk.foreach_fanin(n,
                                  [&](const auto fi)
                                  {
                                      const auto fn = ntk.get_node(fi);
                                      duplication_cost[n] += duplication_cost[fn];
                                  });
            });
    }

    uint32_t tfi_size_unique(mockturtle::node<Ntk> root)
    {
        std::deque<mockturtle::node<Ntk>>         stack;
        std::unordered_set<mockturtle::node<Ntk>> visited;

        stack.push_back(root);

        while (!stack.empty())
        {
            auto n = stack.back();
            stack.pop_back();

            if (!visited.insert(n).second)
            {
                continue;
            }

            ntk.foreach_fanin(n, [&](auto const& f) { stack.push_back(ntk.get_node(f)); });
        }

        // If you want TFI excluding the root itself:
        return visited.size();
    }

    [[nodiscard]] uint64_t gate_cross_cost() const noexcept
    {
        uint64_t gate_cross_cost = 0;
        if (ps.xor_gates)
        {
            if (ps.buffer)
            {
                gate_cross_cost = 7;
            }
            else
            {
                gate_cross_cost = 5;
            }
        }
        else
        {
            if (ps.buffer)
            {
                gate_cross_cost = 48;
            }
            else
            {
                gate_cross_cost = 17;
            }
        }
        return gate_cross_cost;
    }

    [[nodiscard]] uint64_t levels_per_crossing() const noexcept
    {
        uint64_t levels_per_crossing = 0;
        if (ps.xor_gates)
        {
            levels_per_crossing = 3;
        }
        else
        {
            levels_per_crossing = 12;
        }
        return levels_per_crossing;
    }

    [[nodiscard]] uint64_t compute_gate_cost(const std::vector<mockturtle::node<Ntk>>& level_v, const std::vector<mockturtle::node<Ntk>>& next_level_v, uint32_t lvl)
    {
        const auto cross_item = ncross_fanins(level_v, next_level_v, lvl);

        if (cross_item.n_crossings > 1000u)
        {
            return std::numeric_limits<uint64_t>::max();
        }

        uint64_t gate_crossing_cost = 0;
        // add cost for all crossings
        gate_crossing_cost += cross_item.n_crossings * gate_cross_cost();
        // add cost for all additional buffers
        if (ps.buffer && cross_item.n_crossings > 0)
        {
            for (const auto& edge : cross_item.crossings_per_edge)
            {
                auto ground_to_cover = cross_item.max_level - edge.second;
                gate_crossing_cost += ground_to_cover * levels_per_crossing();
            }
        }

        return gate_crossing_cost;
    }

    template <typename T>
    uint32_t count_total_duplications(std::vector<T> const& v)
    {
        std::unordered_map<T, uint32_t> freq;
        for (auto const& x : v)
        {
            ++freq[x];
        }

        uint32_t dups = 0;
        for (auto const& [val, count] : freq)
        {
            if (count > 1)
            {
                dups += (count - 1);
            }
        }
        return dups;
    }

    template <typename T>
    uint32_t duplication_total_cost(std::vector<T> const& v)
    {
        std::unordered_map<T, uint32_t> freq;
        for (auto const& x : v)
        {
            ++freq[x];
        }

        uint32_t total_cost = 0;

        for (auto const& [node, count] : freq)
        {
            if (count > 1)
            {
                const uint32_t extra = count - 1;
                total_cost += extra * static_cast<uint32_t>(tfi_size_unique(node));
            }
        }

        return total_cost;
    }

    template <typename T>
    uint32_t duplication_tfi_cost(std::vector<T> const& level_v)
    {
        std::unordered_map<T, uint32_t> freq;
        // collect duplicated nodes std::unordered_map<mockturtle::node<Ntk>, uint32_t> freq;
        for (auto const& n : level_v)
        {
            ++freq[n];
        }

        std::vector<mockturtle::node<Ntk>> dup_nodes;
        for (auto const& [n, c] : freq)
        {
            if (c > 1)
            {
                dup_nodes.push_back(n);
            }
        }

        if (dup_nodes.empty())
        {
            return 0;
        }

        ntk.incr_trav_id();
        auto const tid = ntk.trav_id();

        std::unordered_set<mockturtle::node<Ntk>> frontier;

        for (auto const& root : dup_nodes)
        {
            std::deque<std::pair<mockturtle::node<Ntk>, uint32_t>> stack;
            stack.emplace_back(root, 0u);

            while (!stack.empty())
            {
                auto const [n, depth] = stack.back();
                stack.pop_back();

                if (ntk.visited(n) == tid)
                {
                    continue;
                }
                ntk.set_visited(n, tid);

                if (ntk.is_pi(n) || depth >= 10u)
                {
                    frontier.insert(n);
                    continue;
                }

                ntk.foreach_fanin(n, [&](auto const& f) { stack.emplace_back(ntk.get_node(f), depth + 1u); });
            }
        }

        uint32_t total_cost = 0;
        for (auto const& n : frontier)
        {
            total_cost += static_cast<uint32_t>(tfi_size_unique(n));
        }

        return total_cost;
    }

    template <typename T>
    uint32_t recombination(std::vector<T> const& level_v)
    {
        std::unordered_map<T, uint32_t> freq;
        for (auto const& n : level_v)
        {
            ++freq[n];
        }

        std::vector<mockturtle::node<Ntk>> dup_nodes;
        dup_nodes.reserve(freq.size());
        for (auto const& [n, c] : freq)
        {
            if (c > 1)
            {
                dup_nodes.push_back(static_cast<mockturtle::node<Ntk>>(n));
                std::cout << "Duplicated node: " << n << "tfi cost : " << tfi_size_unique(n) << std::endl;
            }
        }

        if (dup_nodes.empty())
        {
            return 0;
        }

        std::unordered_map<mockturtle::node<Ntk>, uint32_t> fanout_count;
        std::unordered_map<mockturtle::node<Ntk>, uint32_t> fanout_depth;

        // For each duplicated root: bounded TFI (depth 10), skipping reconvergence within that root only.
        for (auto const& root : dup_nodes)
        {
            ntk.incr_trav_id();
            auto const tid = ntk.trav_id();

            std::deque<std::pair<mockturtle::node<Ntk>, uint32_t>> stack;
            stack.emplace_back(root, 0u);

            while (!stack.empty())
            {
                auto const [n, depth] = stack.back();
                stack.pop_back();

                if (ntk.visited(n) == tid)
                {
                    continue; // reconvergent path within this root cone
                }
                ntk.set_visited(n, tid);

                if (ntk.is_fanout(n))
                {
                    // this increments at most once per root due to trav_id
                    auto const new_count = ++fanout_count[n];

                    // since the network is balanced, any later encounter would have same depth anyway
                    if (new_count == 1u)
                    {
                        fanout_depth.emplace(n, depth);
                    }
                }

                if (ntk.is_pi(n) || depth >= 10u)
                {
                    continue;
                }

                ntk.foreach_fanin(n, [&](auto const& f) {
                    stack.emplace_back(ntk.get_node(f), depth + 1u);
                });
            }
        }

        // printing
        for (auto const& [n, count] : fanout_count)
        {
            auto const d = fanout_depth.at(n);
            std::cout << "Fanout node " << n << " reached from " << count << " duplicated roots"
                      << " , depth: " << d << "\n";
        }

        std::cout << "Width of vector: " << level_v.size() << std::endl;

        uint32_t total_cost = 0;

        return total_cost;
    }

    template <typename T>
    void remove_duplications_keep_order_inline(std::vector<T>& v)
    {
        std::unordered_set<T> seen;
        std::vector<T>        out;
        out.reserve(v.size());

        for (auto const& x : v)
        {
            if (seen.insert(x).second)  // true if inserted (not seen before)
            {
                out.push_back(x);
            }
        }

        v = std::move(out);
    }

    template <typename T>
    std::vector<T> remove_duplications_keep_order(const std::vector<T>& in)
    {
        std::unordered_set<T> seen;
        std::vector<T>        out;
        out.reserve(in.size());

        for (auto const& x : in)
        {
            if (seen.insert(x).second)
            {
                out.push_back(x);
            }
        }

        return out;
    }

    uint64_t count_level_crossings(uint32_t r0, uint32_t r1)
    {
        mockturtle::fanout_view<Ntk> fanout_ntk{ntk};
        if (r0 >= fanout_ntk.depth() || r1 > fanout_ntk.depth() || r1 != r0 + 1)
        {
            return 0;
        }

        uint64_t              total = 0;
        std::vector<uint64_t> penalty(fanout_ntk.rank_width(r1) + 1, 0);
        uint64_t              max_pos = 0;

        fanout_ntk.foreach_node_in_rank(r0,
                                        [&](auto const& n)
                                        {
                                            std::vector<uint64_t> targets;
                                            targets.reserve(ntk.fanout_size(n));

                                            fanout_ntk.foreach_fanout(
                                                n,
                                                [&](auto const& fo)
                                                {
                                                    if (fanout_ntk.level(fo) == r1)  // ensure edge goes to r1
                                                    {
                                                        targets.push_back(fanout_ntk.rank_position(fo));
                                                    }
                                                });

                                            for (const auto pos : targets)
                                            {
                                                for (auto k = pos + 1; k <= max_pos; ++k)
                                                {
                                                    total += penalty[k];
                                                }
                                            }

                                            for (const auto pos : targets)
                                            {
                                                max_pos = std::max(max_pos, pos);
                                                penalty[pos]++;
                                            }
                                        });

        return total;
    }

    [[nodiscard]] virtual_pi_network<Ntk> run()
    {
        assign_duplicaton_costs();
        // Initialize the POs with foreach_node to retain the rank_view order
        std::vector<mockturtle::node<Ntk>> pos{};
        pos.reserve(ntk.num_pos());
        ntk.foreach_node(
            [this, &pos](const auto n)
            {
                if (ntk.is_po(n))
                {
                    const auto po = ntk.get_node(n);
                    if (std::find(pos.begin(), pos.end(), po) == pos.end())
                    {
                        pos.push_back(po);
                    }
                }
            });

        // Randomize the PO order
        if (ps.po_order == planarization_params::output_order::RANDOM_PO_ORDER)
        {
            // Generate a random engine
            static std::mt19937_64 generator(std::random_device{}());
            // Shuffle the pos vector
            std::shuffle(pos.begin(), pos.end(), generator);
        }

        // save the nodes of the next level
        std::vector<mockturtle::node<Ntk>> next_level{};
        next_level.reserve(pos.size());

        // Process the first level
        for (const auto& po : pos)
        {
            fis.clear();
            compute_slice_delays(po);
            next_level.push_back(po);
        }

        ntk_lvls.push_back(next_level);
        next_level.clear();

        next_level = compute_node_order();

        // check if the final/PI level is reached
        bool f_final_level = check_final_level(next_level);

        uint32_t cross_lvl = 0;
        // std::cout << "Num crossings" << count_level_crossings(5, 6) << std::endl;

        bool once = true;
        // Process all other levels
        while (!next_level.empty() && !f_final_level)
        {
            // Count duplications in the level
            auto dups     = count_total_duplications(next_level);

            if (dups > 0)
            {
                // Here the first and last level are not considered
                const auto lvl = ntk.depth() - ntk_lvls.size();
                auto next_level_v = remove_duplications_keep_order(next_level);
                auto dup_cost = duplication_total_cost(next_level);
                auto cross_cost = compute_gate_cost(ntk_lvls[ntk_lvls.size()-1], next_level_v, lvl+1);

                if (cross_cost < dup_cost)
                {
                    // recombination(next_level);
                    /*uint64_t fanout_count = 0;
                    ntk.foreach_node_in_rank(lvl-1,
                                             [&](auto const& n)
                                             {
                                                 if (ntk.is_fanout(n))
                                                 {
                                                     fanout_count++;
                                                 }
                                             });
                    std::cout << "Fanout count of preceding level: " << fanout_count << std::endl;*/

                    std::cout << "Duplications for level " << lvl << ": " << dups << std::endl;
                    std::cout << "Duplication cost: " << dup_cost << std::endl;
                    std::cout << "cross_cost: " << cross_cost << std::endl;

                    next_level = next_level_v;
                    once = false;
                }
                cross_lvl = ntk.depth() - ntk_lvls.size();
            }
            // Push the level to the node array
            ntk_lvls.push_back(next_level);
            lvl_pairs.clear();

            // Store the nodes of the next level
            for (const auto& cur_node : next_level)
            {
                fis.clear();

                // There is one slice in the H-Graph for each node in the level
                compute_slice_delays(cur_node);
            }
            // Clear before starting computations on the next level
            next_level.clear();
            // Compute the next level
            next_level = compute_node_order();
            // Check if we are at the final level
            f_final_level = check_final_level(next_level);
        }
        // Push the final level (PIs)
        if (f_final_level)
        {
            ntk_lvls.push_back(next_level);
        }

        std::vector<std::vector<mockturtle::node<virtual_pi_network<Ntk>>>> ntk_lvls_new{};

        /*for (uint32_t i = 0; i < ntk_lvls.size(); ++i)
        {
            auto dups = count_total_duplications(ntk_lvls[i]);

            std::cout << "Duplications for level " << ntk_lvls.size() - i - 1 << ": " << dups << std::endl;

            /*if (dups > 0)
            {
                // remove_duplications_keep_order( ntk_lvls[i] );
                break;
            }#1#
        }*/

        // create virtual pi network
        auto virtual_ntk = create_virtual_pi_ntk_from_duplicated_nodes_with_keep_original_levels(
            ntk, ntk_lvls, ntk_lvls_new, cross_lvl);

        // the ntk_levels were created in reverse order
        std::reverse(ntk_lvls_new.begin(), ntk_lvls_new.end());

        // assign the ranks in the virtual network based on ntk_lvls_new
        virtual_ntk.update_ranks();
        virtual_ntk.set_all_ranks(ntk_lvls_new);

        // restore possibly set signal names
        restore_network_name(ntk, virtual_ntk);
        restore_output_names(ntk, virtual_ntk);

        return virtual_ntk;
    }

  private:
    /**
     * The input network.
     */
    Ntk ntk{};
    /**
     * The fanout_view of the input network.
     */
    mockturtle::fanout_view<Ntk> fanout_ntk{};
    /**
     * The currently node_pairs used in the current level.
     */
    std::vector<std::vector<hgraph_node<Ntk>>> lvl_pairs{};
    /**
     * The fanin nodes.
     */
    std::vector<mockturtle::node<Ntk>> fis{};
    /**
     * The network stored as levels.
     */
    std::vector<std::vector<mockturtle::node<Ntk>>> ntk_lvls{};
    /**
     * The stats of the planarization class.
     */
    planarization_params ps{};
    /**
     * The duplication cost for nodes.
     */
    std::unordered_map<mockturtle::node<Ntk>, uint32_t> duplication_cost{};
    /**
     * The moving cost for nodes.
     */
    std::unordered_map<mockturtle::node<Ntk>, uint32_t> moving_cost{};
    /**
     * The container saving all crossings in order to compute the cost for inserting gate crossings.
     */
    std::vector<stage_result> crossing_ctn{};
};

}  // namespace detail

template <typename NtkSrc>
[[nodiscard]] virtual_pi_network<NtkSrc> planarization(const NtkSrc& ntk_src, planarization_params ps = {})
{
    static_assert(mockturtle::is_network_type_v<NtkSrc>, "NtkSrc is not a network type");
    static_assert(mockturtle::has_create_node_v<NtkSrc>, "NtkSrc does not implement the create_node function");
    static_assert(mockturtle::has_rank_position_v<NtkSrc>, "NtkSrc does not implement the rank_position function");

    if (!is_balanced(ntk_src))
    {
        throw std::invalid_argument("Networks have to be balanced for this duplication");
    }

    detail::planarization_impl p{ntk_src, ps};

    auto result = p.run();

    // check for planarity
    /*mincross_stats  st_min{};
    mincross_params p_min{};
    p_min.optimize = false;

    auto ntk_min = mincross(result, p_min, &st_min);  // counts crossings
    if (st_min.num_crossings != 0)
    {
        throw std::runtime_error("Planarization failed: resulting network is not planar");
    }*/

    return result;
}

}  // namespace fiction

#endif  // FICTION_PLANARIZATION_HPP
