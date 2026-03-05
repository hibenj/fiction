//
// Created by benjamin on 2/11/26.
//

#ifndef FICTION_NODE_DUPLCATION_PLANARIZATION_F_HPP
#define FICTION_NODE_DUPLICATION_PLANARIZATION_F_HPP

#include "fiction/algorithms/graph/mincross.hpp"
#include "fiction/algorithms/network_transformation/network_balancing.hpp"
#include "fiction/algorithms/network_transformation/node_duplication_planarization.hpp"
#include "fiction/networks/virtual_pi_network.hpp"
#include "fiction/utils/debug/network_writer.hpp"

#include <mockturtle/traits.hpp>
#include <mockturtle/utils/node_map.hpp>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <limits>
#include <random>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace fiction
{

/**
 * Parameters for the node duplication algorithm.
 */
struct node_duplication_planarization_f_params
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
     * Controls the heuristic used to reorder the next level before cost evaluation.
     */
    enum class crossing_minimization : uint8_t
    {
        /**
         * Single-pass barycenter ordering.
         */
        BASELINE,
        /**
         * Barycenter ordering followed by a bounded adjacent-swap improvement pass.
         */
        ADJACENT_SWAP
    };
    /**
     * The output order used. Defaults to KEEP_PO_ORDER.
     */
    output_order po_order = output_order::KEEP_PO_ORDER;
    /**
     * The crossing minimization heuristic. Defaults to ADJACENT_SWAP.
     */
    crossing_minimization cross_min = crossing_minimization::ADJACENT_SWAP;
    /**
     * Whether the otput network should be buffered.
     */
    bool buffer = true;
    /**
     * Whether xor gates are allowed in the network.
     */
    bool xor_gates = false;
    /**
     * Whether xor gates are allowed in the network.
     */
    bool cross_gates = false;

    uint32_t duplication_level_bias = 0u;

    uint32_t duplication_level_weight = 0u;

    uint32_t duplication_current_level_weight = 0u;
};

/**
 * Represents one node in the H-graph used for crossing minimization.
 *
 * For a node in level l of the input network, all possible orderings of its fanins from layer l−1 are enumerated.
 * Each such ordering is represented by an H-graph node. The first and last fanins of the ordering are stored, since
 * these determine the delay in the H-graph. The remaining fanins are placed in middle. Their mutual order is irrelevant
 * for this algorithm.
 *
 * @tparam Ntk Network type from which node types are drawn.
 */
template <typename Ntk>
struct hgraph_node_f
{
    /**
     * The root node.
     */
    mockturtle::node<Ntk> root;
    /**
     * First and last fanin.
     */
    std::pair<mockturtle::node<Ntk>, mockturtle::node<Ntk>> outer_fanins;
    /**
     * All remaining fanins.
     */
    std::vector<mockturtle::node<Ntk>> middle_fanins;
    /**
     * Specifies the delay value for the hgraph_node_f.
     */
    uint64_t delay;
    /**
     * Index of the predecessor H-graph node.
     */
    std::size_t fanin_it{};
    /**
     * Constructs an H-graph node with given first and last fanins and delay.
     *
     * @param n
     * @param first The first (leftmost) fanin in the ordering.
     * @param last  The last (rightmost) fanin in the ordering.
     * @param delay_value The delay value for the node.
     */
    hgraph_node_f(mockturtle::node<Ntk> n, const mockturtle::node<Ntk>& first, const mockturtle::node<Ntk>& last,
                  const uint64_t delay_value) :
            root(n),
            outer_fanins(first, last),
            delay(delay_value)
    {}
};

/**
 * Calculates pairs of nodes from a given vector of nodes.
 *
 * This function takes a vector of nodes and returns a vector of node pairs. Each node pair consists of two nodes from
 * the input vector and an optional vector of middle nodes. The delay of each node pair is initialized to infinity.
 *
 * @tparam Ntk The network type.
 * @param nodes The vector of nodes.
 * @return The vector of node pairs.
 */
template <typename Ntk>
[[nodiscard]] std::vector<hgraph_node_f<Ntk>>
calculate_pairs_f(mockturtle::node<Ntk> root, const std::vector<mockturtle::node<Ntk>>& nodes) noexcept
{
    std::vector<hgraph_node_f<Ntk>> pairwise_combinations{};
    pairwise_combinations.reserve(nodes.size() * (nodes.size() - 1));

    if (nodes.size() == 1)
    {
        const hgraph_node_f<Ntk> pair = {root, nodes[0], nodes[0],
                                         std::numeric_limits<uint64_t>::max()};  // Initialize delay to inf
        pairwise_combinations.push_back(pair);
        return pairwise_combinations;
    }

    for (auto it1 = nodes.cbegin(); it1 != nodes.cend(); ++it1)
    {
        for (auto it2 = it1 + 1; it2 != nodes.cend(); ++it2)
        {
            std::vector<mockturtle::node<Ntk>> middle_fanins{};
            middle_fanins.reserve(nodes.size() - 2);

            // fill middle_fanins with non-pair members
            for (auto it = nodes.cbegin(); it != nodes.cend(); ++it)
            {
                if (it != it1 && it != it2)
                {
                    middle_fanins.push_back(*it);
                }
            }

            hgraph_node_f<Ntk> pair1 = {root, *it1, *it2,
                                        std::numeric_limits<uint64_t>::max()};  // Initialize delay to inf
            hgraph_node_f<Ntk> pair2 = {root, *it2, *it1,
                                        std::numeric_limits<uint64_t>::max()};  // Initialize delay to inf

            // Add middle_fanins to pairs
            pair1.middle_fanins = middle_fanins;
            pair2.middle_fanins = middle_fanins;

            pairwise_combinations.push_back(pair1);
            pairwise_combinations.push_back(pair2);
        }
    }

    return pairwise_combinations;
}

namespace detail
{

template <typename Ntk>
using levelized_node_order = std::vector<std::vector<mockturtle::node<Ntk>>>;

template <typename Ntk>
class node_duplication_planarization_f_impl
{
  public:
    [[maybe_unused]] node_duplication_planarization_f_impl(const Ntk&                                     src,
                                                           const node_duplication_planarization_f_params& p) :
            ntk(src),
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
    void compute_slice_delays(const mockturtle::node<Ntk>& n)
    {
        // Pis need to be propagated into the next level, since they have to be connected without crossings
        if (ntk.is_pi(dupl2old[n]))
        {
            fis.push_back(dupl2old[n]);
        }

        // Respect the rank order. If two combinations have the same delay and have no seen advantage then the one from
        // the original ranking is used, since it is inserted and not overwritten afterward.
        ntk.foreach_fanin(dupl2old[n],
                          [&](auto fi)
                          {
                              if (!ntk.is_constant(fi))
                              {
                                  auto nd = ntk.get_node(fi);

                                  auto it =
                                      std::lower_bound(fis.begin(), fis.end(), nd, [&](auto const& a, auto const& b)
                                                       { return ntk.rank_position(a) < ntk.rank_position(b); });

                                  fis.insert(it, nd);
                              }
                          });

        assert(!fis.empty() && "There has to be at least one node in this level");

        // Compute the combinations in one slice
        auto combinations = calculate_pairs_f<Ntk>(n, fis);
        assert(!combinations.empty() && "Combinations are empty. There might be a dangling node");

        if (!lvl_pairs.empty())
        {
            std::vector<hgraph_node_f<Ntk>>* combinations_last = &lvl_pairs.back();

            for (std::size_t cur_idx = 0; cur_idx < combinations.size(); ++cur_idx)
            {
                auto& node_pair_cur = combinations[cur_idx];

                for (std::size_t last_idx = 0; last_idx < combinations_last->size(); ++last_idx)
                {
                    auto& node_pair_last = (*combinations_last)[last_idx];

                    // If there is a connection between the two node pairs the delay is calculated like this
                    if (node_pair_cur.outer_fanins.first == node_pair_last.outer_fanins.second &&
                        node_pair_last.delay + 1 < node_pair_cur.delay)
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
                        // This solves equal path delays, if they are connected in the next layer
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
     * element of the vector.
     *
     * @param node The node to be inserted.
     * @param vec The vector to insert the node into.
     */
    void insert_if_not_first(mockturtle::node<Ntk> node, std::vector<mockturtle::node<Ntk>>& vec,
                             std::vector<mockturtle::node<Ntk>>& fanins)
    {
        if (vec.empty() || dupl2old[vec.front()] != node)
        {
            if (std::find(vec.begin(), vec.end(), node) == vec.end())
            {
                // node is not a duplicate -> just add to next_level and to fanins
                vec.insert(vec.begin(), node);
                fanins.push_back(node);
                dupl2old[node] = node;
            }
            else
            {
                // node is a duplicate -> add new node to next level and fanins
                mockturtle::node<Ntk> new_node = ntk.size() + num_dupl_nodes++;
                vec.insert(vec.begin(), new_node);
                fanins.push_back(new_node);
                // track new node
                dupl2old[new_node] = node;
            }
        }
        else if (dupl2old[vec.front()] == node)
        {
            fanins.push_back(vec.front());
        }
    }

    void add_combination(const hgraph_node_f<Ntk>& combination, std::vector<mockturtle::node<Ntk>>& next_level)
    {
        // init fanin vector
        std::vector<mockturtle::node<Ntk>> fanins{};

        // Insert the terminal node
        insert_if_not_first(combination.outer_fanins.second, next_level, fanins);

        // Insert middle_fanins
        for (const auto& node : combination.middle_fanins)
        {
            insert_if_not_first(node, next_level, fanins);
        }

        // Insert the first node
        if (combination.outer_fanins.second != combination.outer_fanins.first)
        {
            insert_if_not_first(combination.outer_fanins.first, next_level, fanins);
        }

        // insert the fanins to the root (the root should be unique this way)
        /*uint32_t key = combination.root;
        std::cout << "Key: " << key << " \n ";
        std::cout << "Fanins: ";
        for (const auto& f : fanins)
        {
            std::cout << f << " ";
        }
        std::cout << "\n";*/

        old2new_fis[combination.root] = fanins;
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
        const auto&                        combinations = lvl_pairs.back();

        // Select the path with the least delay and follow it via fanin relations
        const auto minimum_it = std::min_element(combinations.cbegin(), combinations.cend(),
                                                 [](const hgraph_node_f<Ntk>& a, const hgraph_node_f<Ntk>& b)
                                                 { return a.delay < b.delay; });

        if (minimum_it != combinations.cend())
        {
            const auto& min_combination = *minimum_it;

            add_combination(min_combination, next_level);

            // Start with index instead of pointer
            std::size_t level    = lvl_pairs.size() - 1;
            std::size_t fanin_it = minimum_it->fanin_it;

            // Follow chain while index is valid
            while (level > 0 && fanin_it < lvl_pairs[level - 1].size())
            {
                const auto& fanin_combination = lvl_pairs[level - 1][fanin_it];
                add_combination(fanin_combination, next_level);

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
            if (!ntk.is_pi(dupl2old[nd]))
            {
                return false;
            }
        }
        return true;
    }

    virtual_pi_network<Ntk> create_virtual_ntk()
    {
        // initialize a network copy and map
        virtual_pi_network<Ntk>                                            duplicated_network{};
        std::unordered_map<mockturtle::node<Ntk>, mockturtle::signal<Ntk>> old2new{};

        // create_pis correctly for equivalence
        // this makes sure old2new[2] = 2, ol2new[3] = 3, ...
        ntk.foreach_pi_unranked(
            [this, &old2new, &duplicated_network](const auto& pi)
            {
                if constexpr (has_is_real_pi_v<Ntk>)
                {
                    if (ntk.is_real_pi(pi))
                    {
                        old2new[pi] = duplicated_network.create_pi();
                    }
                }
                else
                {
                    old2new[pi] = duplicated_network.create_pi();
                }
            });

        std::vector<mockturtle::node<Ntk>> pi_ranks(ntk_lvls[0].size());

        // create and map pis correctly
        std::vector<bool> pi_created(ntk.num_pis() + 2u, false);
        for (auto i = 0u; i < ntk_lvls[0].size(); ++i)
        {
            const auto& pi        = ntk_lvls[0][i];
            const auto  pi_mapped = dupl2old[pi];
            assert(old2new_fis[pi].size() == 0);
            if (!pi_created[dupl2old[pi]])
            {
                pi_created[dupl2old[pi]] = true;
                old2new[pi]              = duplicated_network.make_signal(dupl2old[pi]);
                pi_ranks[i]              = old2new[pi];
                // duplicated_network.on_add(new_sig);
            }
            else
            {
                const auto pi_create = dupl2old[pi];
                const auto new_sig   = duplicated_network.create_virtual_pi(dupl2old[pi]);
                old2new[pi]          = new_sig;
                pi_ranks[i]          = old2new[pi];
                // duplicated_network.on_add(new_sig);
            }
        }

        // start from one level above the PIs, since they are already created
        for (auto i = 1u; i < ntk_lvls.size(); ++i)
        {
            const auto& lvl = ntk_lvls[i];
            for (const auto& nd : lvl)
            {
                auto children = old2new_fis[nd];
                if (!cross_lvls.empty() && i - 1 == cross_lvls.back())
                {
                    for (auto& child : children)
                    {
                        const auto& c = dupl2old[child];
                        child         = old2new[c];
                    }
                }
                else
                {
                    for (auto& child : children)
                    {
                        child = old2new[child];
                    }
                }
                /*for (auto& child : children)
                {
                    child = old2new[child];
                }*/
                // only fill old2new for the last level to track POs
                const auto new_sig = duplicated_network.create_node(children, ntk.node_function(dupl2old[nd]));
                old2new[nd]        = new_sig;
                // duplicated_network.on_add(new_sig);
            }
            if (!cross_lvls.empty() && i - 1 == cross_lvls.back())
            {
                cross_lvls.pop_back();
            }
        }

        // add primary outputs to finalize the network
        ntk.foreach_po(
            [this, &duplicated_network, &old2new](const auto& po)
            {
                const auto tgt_signal = old2new[ntk.get_node(po)];
                const auto tgt_po = ntk.is_complemented(po) ? duplicated_network.create_not(tgt_signal) : tgt_signal;

                duplicated_network.create_po(tgt_po);
            });

        duplicated_network.update_ranks();

        duplicated_network.set_ranks(0, pi_ranks);

        return duplicated_network;
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

    [[nodiscard]] uint64_t weighted_tfi_cost(mockturtle::node<Ntk> root, uint32_t current_level) const
    {
        struct duplication_params
        {
            double base = 1.0;      // constant per node
            double amp  = 1.01;      // scales exponential part

            double r_level = 1.02;  // per-level growth (>1)
            double r_cur   = 1.02;  // global per-current-level growth (>1)

            bool include_root = true;
        };

        duplication_params psd{};

        std::vector<mockturtle::node<Ntk>>        stack;
        std::unordered_set<mockturtle::node<Ntk>> visited;
        stack.push_back(root);

        // Global multiplier depends only on current_level (absolute)
        const double g = std::pow(psd.r_cur, static_cast<double>(current_level));

        double total = 0.0;

        while (!stack.empty())
        {
            const auto n = stack.back();
            stack.pop_back();

            if (!visited.insert(n).second)
            {
                continue;
            }

            if (psd.include_root || n != root)
            {
                const uint32_t lvl = ntk.has_level(n) ? ntk.level(n) : 0u;

                // Absolute level weight
                double wn = 0;
                if (ntk.fanin_size(n) == 1 && ntk.fanout_size(n) == 1)
                {
                    wn = 0.5;
                }
                else
                {
                    wn = psd.base + (psd.amp * std::pow(psd.r_level, static_cast<double>(lvl)));
                }
                total += wn * g;
            }

            ntk.foreach_fanin(n, [&](auto const& f) { stack.push_back(ntk.get_node(f)); });
        }

        if (total <= 0.0)
        {
            return 0u;
        }
        if (total >= static_cast<double>(std::numeric_limits<uint64_t>::max()))
        {
            return std::numeric_limits<uint64_t>::max();
        }

        return static_cast<uint64_t>(std::llround(total));
    }

    template <typename T>
    uint32_t duplication_total_cost(std::vector<T> const& v)
    {
        std::unordered_map<T, uint32_t> freq;
        for (auto const& x : v)
        {
            ++freq[dupl2old[x]];
        }

        uint32_t total_cost = 0;

        for (auto const& [node, count] : freq)
        {
            if (count > 1)
            {
                const uint32_t extra = count - 1;
                total_cost += extra * tfi_size_unique(node);
            }
        }

        return total_cost;
    }

    template<typename T>
    uint32_t duplication_total_cost_fi(std::vector<T> const& v, uint32_t lvl)
    {
        std::unordered_set<T> seen;
        bool                  has_last = false;
        T                     last_fanin{};
        uint32_t              total_cost = 0;

        for (auto const& x : v)
        {
            const auto n = dupl2old[x];

            const auto f = fanins(ntk, n).fanin_nodes[0];

            if (!has_last || f != last_fanin)
            {
                if (seen.find(f) != seen.end())
                {
                    total_cost += weighted_tfi_cost(f, lvl);
                }
                else
                {
                    seen.insert(f);
                }
            }

            last_fanin = f;
            has_last   = true;
        }

        return total_cost;
    }

    template <typename T>
    uint32_t duplication_total_cost2(std::vector<T> const& v, uint32_t lvl)
    {
        std::unordered_map<T, uint32_t> freq;
        for (auto const& x : v)
        {
            ++freq[dupl2old[x]];
        }

        uint32_t total_cost = 0;

        for (auto const& [node, count] : freq)
        {
            if (count > 1)
            {
                const uint32_t extra = count - 1;
                total_cost += extra * weighted_tfi_cost(node, lvl);
            }
        }

        return total_cost;
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

    [[nodiscard]] uint64_t gate_cross_cost() const noexcept
    {
        uint64_t gate_cross_cost = 0;
        if (ps.xor_gates)
        {
            if (ps.buffer)
            {
                gate_cross_cost = 10;
            }
            else
            {
                gate_cross_cost = 6;
            }
        }
        else
        {
            if (ps.buffer)
            {
                gate_cross_cost = 59;
            }
            else
            {
                gate_cross_cost = 27;
            }
        }
        return gate_cross_cost;
    }

    [[nodiscard]] uint64_t levels_per_crossing() const noexcept
    {
        uint64_t levels_per_crossing = 0;
        if (ps.xor_gates)
        {
            levels_per_crossing = 4;
        }
        else
        {
            levels_per_crossing = 14;
        }
        return levels_per_crossing;
    }

    /**
     * @brief Counts edge crossings using fanin direction and next-level vector positions.
     *
     * @param prev_level_v The current level (targets) ordered left-to-right.
     * @param next_level_v The next level (sources) ordered left-to-right.
     * @return The number of crossings between the two levels.
     */
    [[nodiscard]] uint64_t count_crossings_light(const std::vector<mockturtle::node<Ntk>>& prev_level_v,
                                                 const std::vector<mockturtle::node<Ntk>>& next_level_v) const
    {
        if (prev_level_v.empty() || next_level_v.empty())
        {
            return 0u;
        }

        std::size_t max_node = 0u;
        for (const auto n : next_level_v)
        {
            max_node = std::max(max_node, static_cast<std::size_t>(n));
        }

        std::vector<int64_t> pos(max_node + 1u, -1);
        for (std::size_t i = 0u; i < next_level_v.size(); ++i)
        {
            pos[static_cast<std::size_t>(next_level_v[i])] = static_cast<int64_t>(i);
        }

        const auto to_old = [this](mockturtle::node<Ntk> n)
        {
            const auto it = dupl2old.find(n);
            return it == dupl2old.cend() ? n : it->second;
        };

        std::vector<uint64_t> penalty_array(next_level_v.size() + 1u, 0u);
        uint64_t              max_pos   = 0u;
        uint64_t              crossings = 0u;

        for (auto n : prev_level_v)
        {
            n = to_old(n);

            std::vector<uint64_t> targets{};
            targets.reserve(ntk.fanin_size(n));

            ntk.foreach_fanin(n,
                              [&](auto const& fi)
                              {
                                  const auto idx = static_cast<std::size_t>(fi);
                                  if (idx >= pos.size())
                                  {
                                      assert(false);
                                      return;
                                  }
                                  const auto p = pos[idx];
                                  if (p < 0)
                                  {
                                      assert(false);
                                      return;
                                  }
                                  targets.push_back(static_cast<uint64_t>(p));
                              });

            for (const auto p : targets)
            {
                for (auto k = p + 1u; k <= max_pos; ++k)
                {
                    crossings += penalty_array[k];
                }
            }

            for (const auto p : targets)
            {
                max_pos = std::max(max_pos, p);
                ++penalty_array[p];
            }
        }

        return crossings;
    }

    void sort_next_level_by_barycenter(const std::vector<mockturtle::node<Ntk>>& prev_level_v,
                                       std::vector<mockturtle::node<Ntk>>&       next_level_v) const
    {
        if (next_level_v.size() < 2u)
        {
            return;
        }

        std::size_t max_node = 0u;
        for (const auto n : next_level_v)
        {
            max_node = std::max(max_node, static_cast<std::size_t>(n));
        }

        std::vector<int64_t> pos(max_node + 1u, -1);
        for (std::size_t i = 0u; i < next_level_v.size(); ++i)
        {
            pos[static_cast<std::size_t>(next_level_v[i])] = static_cast<int64_t>(i);
        }

        std::vector<double>   sum(next_level_v.size(), 0.0);
        std::vector<uint32_t> count(next_level_v.size(), 0u);

        const auto to_old = [this](mockturtle::node<Ntk> n)
        {
            const auto it = dupl2old.find(n);
            return it == dupl2old.cend() ? n : it->second;
        };

        for (std::size_t t = 0u; t < prev_level_v.size(); ++t)
        {
            auto n = to_old(prev_level_v[t]);
            ntk.foreach_fanin(n,
                              [&](auto const& fi)
                              {
                                  const auto idx = static_cast<std::size_t>(fi);
                                  if (idx >= pos.size())
                                  {
                                      assert(false);
                                      return;
                                  }
                                  const auto p = pos[idx];
                                  if (p < 0)
                                  {
                                      assert(false);
                                      return;
                                  }
                                  sum[static_cast<std::size_t>(p)] += static_cast<double>(t);
                                  ++count[static_cast<std::size_t>(p)];
                              });
        }

        struct order_item
        {
            mockturtle::node<Ntk> node;
            double                key;
            std::size_t           orig_idx;
        };

        std::vector<order_item> items{};
        items.reserve(next_level_v.size());

        for (std::size_t i = 0u; i < next_level_v.size(); ++i)
        {
            const auto key = count[i] > 0u ? (sum[i] / static_cast<double>(count[i])) : static_cast<double>(i);
            items.push_back({next_level_v[i], key, i});
        }

        std::stable_sort(items.begin(), items.end(),
                         [](const order_item& a, const order_item& b)
                         {
                             if (a.key != b.key)
                             {
                                 return a.key < b.key;
                             }
                             return a.orig_idx < b.orig_idx;
                         });

        for (std::size_t i = 0u; i < items.size(); ++i)
        {
            next_level_v[i] = items[i].node;
        }
    }

    void improve_by_adjacent_swaps(const std::vector<mockturtle::node<Ntk>>& prev_level_v,
                                   std::vector<mockturtle::node<Ntk>>& next_level_v, const std::size_t max_swaps) const
    {
        if (next_level_v.size() < 2u || max_swaps == 0u)
        {
            return;
        }

        auto        current = count_crossings_light(prev_level_v, next_level_v);
        std::size_t swaps   = 0u;

        for (std::size_t i = 0u; i + 1u < next_level_v.size() && swaps < max_swaps; ++i)
        {
            std::swap(next_level_v[i], next_level_v[i + 1u]);
            const auto candidate = count_crossings_light(prev_level_v, next_level_v);

            if (candidate < current)
            {
                current = candidate;
                ++swaps;
            }
            else
            {
                std::swap(next_level_v[i], next_level_v[i + 1u]);
            }
        }
    }

    void minimize_crossings(const std::vector<mockturtle::node<Ntk>>& prev_level_v,
                            std::vector<mockturtle::node<Ntk>>&       next_level_v) const
    {
        sort_next_level_by_barycenter(prev_level_v, next_level_v);

        if (ps.cross_min == node_duplication_planarization_f_params::crossing_minimization::ADJACENT_SWAP)
        {
            constexpr std::size_t kMaxSwaps = 32u;
            const auto max_swaps = std::min(kMaxSwaps, next_level_v.size() > 0u ? next_level_v.size() - 1u : 0u);
            improve_by_adjacent_swaps(prev_level_v, next_level_v, max_swaps);
        }
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

    struct stage_result
    {
        uint64_t                               max_level;
        uint32_t                               n_crossings;
        std::vector<std::pair<edge, uint64_t>> crossings_per_edge;
    };

    stage_result ncross_fanins(const std::vector<mockturtle::node<Ntk>>& prev_level_v,
                               const std::vector<mockturtle::node<Ntk>>& next_level_v, uint32_t lvl)
    {
        stage_result result{};

        // We index by *source* position in previous rank (r-1)
        uint64_t                                           prev_width = ntk.rank_width(lvl - 1);
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

        for (auto n : prev_level_v)
        {
            // map duplicated nodes to their original nodes
            n = dupl2old[n];
            // Collect incoming edges (fi -> n) with their source positions
            std::vector<std::pair<uint64_t, edge>> incoming;
            incoming.reserve(ntk.fanin_size(n));  // if available; otherwise remove

            ntk.foreach_fanin(n,
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

    [[nodiscard]] uint64_t compute_gate_cost(const std::vector<mockturtle::node<Ntk>>& prev_level_v,
                                             std::vector<mockturtle::node<Ntk>>& next_level_v, uint32_t lvl)
    {
        minimize_crossings(prev_level_v, next_level_v);
        const auto cross_item = ncross_fanins(prev_level_v, next_level_v, lvl);

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

    [[nodiscard]] virtual_pi_network<Ntk> run()
    {
        // Initialize ntk_lvls
        ntk_lvls.reserve(ntk.size());

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
        if (ps.po_order == node_duplication_planarization_f_params::output_order::RANDOM_PO_ORDER)
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
            dupl2old[po] = po;
            compute_slice_delays(po);
            next_level.push_back(po);
        }

        ntk_lvls.insert(ntk_lvls.begin(), next_level);
        next_level.clear();

        next_level = compute_node_order();

        // check if the final/PI level is reached
        bool f_final_level = check_final_level(next_level);

        bool     once = true;
        uint64_t dups = 0;
        // Process all other levels
        while (!next_level.empty() && !f_final_level)
        {
            if (ps.cross_gates)
            {
                if (dups > 0)
                {
                    // Here the first and last level are not considered
                    const auto lvl              = ntk.depth() - ntk_lvls.size();
                    auto       dup_cost         = duplication_total_cost(next_level);
                    auto       cross_next_level = ntk.get_ranks(lvl);
                    auto       cross_cost       = compute_gate_cost(ntk_lvls[0], cross_next_level, lvl + 1);

                    auto dup_cost_weighted = duplication_total_cost2(next_level, lvl);
                    std::cout << "Duplication cost: " << dup_cost << std::endl;
                    std::cout << "Duplication cost weighted: " << dup_cost_weighted << std::endl;
                    std::cout << "Relation: " << static_cast<double>(dup_cost_weighted) / static_cast<double>(dup_cost) << std::endl;
                    auto dupl_cost_fi = duplication_total_cost_fi(next_level, lvl);
                    std::cout << "Duplication cost fanin: " << dupl_cost_fi << std::endl;
                    std::cout << "Relation: " << static_cast<double>(dupl_cost_fi) / static_cast<double>(dup_cost) << std::endl;

                    if (cross_cost < dup_cost_weighted)
                    {
                        next_level = cross_next_level;
                        cross_lvls.push_back(lvl);
                        once = false;
                        std::cout << "Duplications for level " << lvl << ": " << dups << std::endl;
                        std::cout << "Duplication cost: " << dup_cost << std::endl;
                        std::cout << "cross_cost: " << cross_cost << std::endl;
                    }
                }
            }
            // Push the level to the node array
            ntk_lvls.insert(ntk_lvls.begin(), next_level);
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
            dups = num_dupl_nodes;
            // Compute the next level
            next_level = compute_node_order();
            dups       = num_dupl_nodes - dups;
            // Check if we are at the final level
            f_final_level = check_final_level(next_level);
        }
        // Push the final level (PIs)
        if (f_final_level)
        {
            ntk_lvls.insert(ntk_lvls.begin(), next_level);
        }

        const auto virtual_ntk = create_virtual_ntk();

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
     * The currently node_pairs used in the current level.
     */
    std::vector<std::vector<hgraph_node_f<Ntk>>> lvl_pairs{};
    /**
     * The fanin nodes.
     */
    std::vector<mockturtle::node<Ntk>> fis{};
    /**
     * The network stored as levels.
     */
    levelized_node_order<Ntk> ntk_lvls{};
    /**
     * The stats of the node_duplication class.
     */
    node_duplication_planarization_f_params ps{};
    /**
     * Holds the duplicated nodes fanin relations and a flag for complemented signals.
     */
    std::unordered_map<mockturtle::node<Ntk>, std::vector<mockturtle::node<Ntk>>> old2new_fis{};
    /**
     * Holds the duplicated nodes fanin relations and a flag for complemented signals.
     */
    std::unordered_map<mockturtle::node<Ntk>, mockturtle::node<Ntk>> dupl2old{};
    /**
     * Holds the number of duplicated nodes. Functions as iterator for saving new nodes.
     */
    uint32_t num_dupl_nodes = 0u;
    /**
     * Holds the lvls for which a crossing gate should be inserted.
     */
    std::vector<uint32_t> cross_lvls;
};

}  // namespace detail

/**
 * Implements a planarization mechanism for networks from the paper \"Fabricatable Interconnect and Molecular QCA
 * Circuits\" by Amitabh Chaudhary, Danny Ziyi Chen, Xiaobo Sharon Hu, Michael T. Niemier, Ramprasad Ravichandran and
 * Kevin Whitton in IEEE Transactions on Computer-Aided Design of Integrated Circuits and Systems, Volume 26, 2007.
 *
 * The planarization achieved by this function solves the Node Duplication Crossing Minimization (NDCE) problem by
 * finding the shortest x-y path in the H-graph for every level in the network. An H-graph describes edge relations
 * between two levels in a network, with one level assumed as fixed, starting at the Primary Outputs (POs). By finding
 * the shortest path from the source (x) to the sink (y) in this H-graph, an optimal solution for the NDCE problem for
 * each level is found. The function traverses from the Primary Outputs (POs) towards the Primary Inputs (PIs).
 *
 * @tparam Ntk Source network type.
 * @param ntk Source network to be utilized for the planarization.
 * @param ps Node duplication parameters used in the computation.
 *
 * @return A planarized virtual_pi_network.
 */
template <typename Ntk>
[[nodiscard]] virtual_pi_network<Ntk> node_duplication_planarization_f(const Ntk&                              ntk,
                                                                       node_duplication_planarization_f_params ps = {})
{
    static_assert(mockturtle::is_network_type_v<Ntk>, "NtkSrc is not a network type");
    static_assert(mockturtle::has_create_node_v<Ntk>, "NtkSrc does not implement the create_node function");
    static_assert(mockturtle::has_rank_position_v<Ntk>, "NtkSrc does not implement the rank_position function");

    if (!is_balanced(ntk))
    {
        throw std::invalid_argument("Networks have to be balanced for this duplication");
    }

    detail::node_duplication_planarization_f_impl p{ntk, ps};

    auto result = p.run();

    if (!ps.cross_gates)
    {
        // check for planarity
        mincross_stats  st_min{};
        mincross_params p_min{};
        p_min.optimize = false;

        mincross(result, p_min, &st_min);  // counts crossings
        if (st_min.num_crossings != 0)
        {
            throw std::runtime_error("Planarization failed: resulting network is not planar");
        }
    }

    return result;
}

}  // namespace fiction
#endif  // FICTION_NODE_DUPLCATION_PLANARIZATION_F_HPP
