//
// Created by benjamin on 11.06.24.
//

#ifndef FICTION_NODE_DUPLICATION_PLANARIZATION_HPP
#define FICTION_NODE_DUPLICATION_PLANARIZATION_HPP

#include "fiction/algorithms/properties/check_planarity.hpp"
#include "fiction/networks/views/extended_rank_view.hpp"
#include "fiction/networks/virtual_pi_network.hpp"

#include <mockturtle/traits.hpp>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <random>
#include <unordered_map>
#include <utility>
#include <vector>

#if (PROGRESS_BARS)
#include <mockturtle/utils/progress_bar.hpp>
#endif

namespace fiction
{

/**
 * Parameters for the node duplication algorithm.
 */
struct node_duplication_planarization_params
{
    /**
     * The output order determines the starting layer for this algorithm. If this option is turned off, the output order
     * remains the same as in the provided network. If it is turned on, the outputs are ordered randomly.
     */
    bool random_output_order = false;
};

namespace detail
{

/**
 * A structure representing a pair of nodes in an H-graph.
 *
 * The nodes stored in this struct describe the fanin-edges of a node in an H-graph.
 * A node pair object holds two nodes, which are saved in the member 'pair'.
 * These two outer nodes are connected through zero or more 'middle_nodes'.
 * The fanin order starts with the first node in 'pair', then proceeds through the 'middle_nodes', and ends with the
 * second node in 'pair'. The order of 'middle_nodes' is arbitrary as they cannot be further connected to any other
 * nodes. For the planarization, only the nodes inside the 'pair' are relevant.
 *
 * @tparam Ntk Network type for the nodes in the pair.
 */
template <typename Ntk>
struct node_pair
{
    /**
     * Defines the beginning and end of the fanin-edged node.
     */
    std::pair<mockturtle::node<Ntk>, mockturtle::node<Ntk>> pair;
    /**
     * Contains the nodes between the fanin-edges node; cannot be connected to any other node.
     */
    std::vector<mockturtle::node<Ntk>> middle_nodes;
    /**
     * Shared pointer to another instance of node_pair detailing fanin-edge alignment.
     */
    node_pair<Ntk>* fanin_pair;
    /**
     * Specifies the delay value for the node.
     */
    uint64_t delay;
    /**
     * Standard constructor.
     *
     * @param node1 The first node of the fanin-edged node.
     * @param node2 The second node of the fanin-edged node.
     * @param delayValue The delay value for the node.
     */
    node_pair(mockturtle::node<Ntk> node1, mockturtle::node<Ntk> node2, uint64_t delayValue) :
            pair(node1, node2),
            delay(delayValue),
            fanin_pair(nullptr)
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
[[nodiscard]] std::vector<node_pair<Ntk>> calculate_pairs(const std::vector<mockturtle::node<Ntk>>& nodes) noexcept
{
    std::vector<node_pair<Ntk>> pairwise_combinations{};
    pairwise_combinations.reserve(nodes.size() * (nodes.size() - 1));

    if (nodes.size() == 1)
    {
        const node_pair<Ntk> pair = {nodes[0], nodes[0],
                                     std::numeric_limits<uint64_t>::max()};  // Initialize delay to inf
        pairwise_combinations.push_back(pair);
        return pairwise_combinations;
    }

    for (auto it1 = nodes.cbegin(); it1 != nodes.cend(); ++it1)
    {
        for (auto it2 = it1 + 1; it2 != nodes.cend(); ++it2)
        {
            std::vector<mockturtle::node<Ntk>> middle_nodes{};
            middle_nodes.reserve(nodes.size() - 2);

            // fill middle_nodes with non-pair members
            for (auto it = nodes.cbegin(); it != nodes.cend(); ++it)
            {
                if (it != it1 && it != it2)
                {
                    middle_nodes.push_back(*it);
                }
            }

            node_pair<Ntk> pair1 = {*it1, *it2, std::numeric_limits<uint64_t>::max()};  // Initialize delay to inf
            node_pair<Ntk> pair2 = {*it2, *it1, std::numeric_limits<uint64_t>::max()};  // Initialize delay to inf

            // Add middle_nodes to pairs
            pair1.middle_nodes = middle_nodes;
            pair2.middle_nodes = middle_nodes;

            pairwise_combinations.push_back(pair1);
            pairwise_combinations.push_back(pair2);
        }
    }

    return pairwise_combinations;
}

template <typename Ntk>
class node_duplication_planarization_impl
{
  public:
    node_duplication_planarization_impl(const Ntk& src, const node_duplication_planarization_params& p) :
            ntk(src),
            ps{p}
    {}

    /**
     * Computes the delay in a given slice (each possible order of node_pairs) of an H-graph.
     *
     * This function iterates over the fanins of the given node and computes the delay for all possible orders
     * of these nodes that form a node_pair. The delay computation depends on the node's connections and position
     * within the graph. If there is a connection between two node_pairs, the delay is incremented by 1. If not,
     * the delay is incremented by 2. Default delay for the first node is 1. If a node_pair doesn't have a connection
     * and its delay (when increased by two) is less than the existing delay, then this node_pair's delay is updated.
     *
     * The processed node_pairs are pushed back to the 'lvl_pairs' data member for subsequent delay calculations.
     *
     * @param nd Node in the H-graph.
     * @param border_pis A boolean indicating whether the input PIs (Primary Inputs) should be propagated to the next
     * level.
     */
    void compute_slice_delays(const mockturtle::node<Ntk>& nd, const bool border_pis)
    {
        // Pis need to be propagated into the next level, since they have to be connected without crossings
        if (ntk.is_pi(nd) && border_pis)
        {
            fis.push_back(nd);
        }

        ntk.foreach_fanin(nd,
                          [this](auto fi)
                          {
                              if (!ntk.is_constant(fi))
                              {
                                  fis.push_back(fi);
                              }
                          });

        assert(!fis.size() == 0 && "Node is a buffered PI that is a PO");
        // Compute the combinations in one slice
        auto combinations = calculate_pairs<Ntk>(fis);
        assert(!combinations.empty() && "Combinations are empty. There might be a dangling node");

        if (!lvl_pairs.empty())
        {
            std::vector<node_pair<Ntk>>* combinations_last = &lvl_pairs.back();

            for (auto& node_pair_cur : combinations)
            {
                for (auto& node_pair_last : *combinations_last)
                {
                    // If there is a connection between the two node pairs the delay is calculated like this
                    if ((node_pair_cur.pair.first == node_pair_last.pair.second &&
                         node_pair_last.delay + 1 < node_pair_cur.delay))
                    {
                        node_pair_cur.fanin_pair = &node_pair_last;
                        node_pair_cur.delay      = node_pair_last.delay + 1;
                    }
                    // If there is no connection between the two node pairs the delay is calculated like this
                    else if (node_pair_last.delay + 2 < node_pair_cur.delay)
                    {
                        node_pair_cur.fanin_pair = &node_pair_last;
                        node_pair_cur.delay      = node_pair_last.delay + 2;
                    }
                    else if (node_pair_last.delay + 2 == node_pair_cur.delay)
                    {
                        // ToDo: If order doesnt matter, decide on a minimal crossing view (implement mincross.c from
                        // graphviz)

                        // this solves equal paths, if they are connected in the next layer via a fanout
                        const auto fc0 = fanins(ntk, node_pair_last.pair.first);
                        if (node_pair_last.fanin_pair != nullptr)
                        {
                            const auto fc1 = fanins(ntk, node_pair_last.fanin_pair->pair.second);
                            for (const auto f0 : fc0.fanin_nodes)
                            {
                                for (const auto f1 : fc1.fanin_nodes)
                                {
                                    if (f0 == f1)
                                    {
                                        node_pair_cur.fanin_pair = &node_pair_last;
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }
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
     * element of the vector. If the vector is not empty and the node is equal to the first element, it does nothing.
     *
     * @param node The node to be inserted.
     * @param vec The vector to insert the node into.
     */
    void insert_if_not_first(const mockturtle::node<Ntk>& node, std::vector<mockturtle::node<Ntk>>& vec,
                             int& saturated_fanout_flag, int position)
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
                saturated_fanout_flag = 1;
            }
        }
    }

    /**
     * Computes the order of nodes in the next level based on delay
     *
     * This function computes the order of nodes in the next level based on their delay in the H-graph of the level. It
     * selects the path with the least delay from the current level pairs and follows it via fanin relations. The nodes
     * are inserted into the next level vector in the order they are encountered.
     *
     * @param next_level The vector to store the nodes in the next level.
     */
    void compute_node_order_next_level(std::vector<mockturtle::node<Ntk>>& next_level)
    {
        int         saturated_fanout_flag = 0;
        const auto& combinations          = lvl_pairs.back();
        // select the path with the least delay and follow it via fanin relations
        const auto minimum_it =
            std::min_element(combinations.cbegin(), combinations.cend(),
                             [](const node_pair<Ntk>& a, const node_pair<Ntk>& b) { return a.delay < b.delay; });
        if (minimum_it != combinations.cend())
        {
            const auto& min_combination = *minimum_it;

            // Insert the terminal node
            insert_if_not_first(min_combination.pair.second, next_level, saturated_fanout_flag, 0);

            // insert middle_nodes
            for (const auto& node : min_combination.middle_nodes)
            {
                insert_if_not_first(node, next_level, saturated_fanout_flag, 1);
            }

            // Insert the first node
            insert_if_not_first(min_combination.pair.first, next_level, saturated_fanout_flag, 1);

            auto fanin_combination = minimum_it->fanin_pair;

            while (fanin_combination)
            {
                // Insert the terminal node
                insert_if_not_first(fanin_combination->pair.second, next_level, saturated_fanout_flag, 0);

                // Insert middle_nodes
                for (const auto& node : fanin_combination->middle_nodes)
                {
                    insert_if_not_first(node, next_level, saturated_fanout_flag, 1);
                }

                // insert the first node
                insert_if_not_first(fanin_combination->pair.first, next_level, saturated_fanout_flag, 1);

                fanin_combination = fanin_combination->fanin_pair;
            }
        }
    }

    /**
     * Checks if the given vector of nodes contains any non-primary inputs.
     *
     * This function iterates through each node in the vector and checks if it is a primary input.
     * If a non-primary input is found, the `f_final_level` parameter is set to false and the loop is exited.
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

    [[nodiscard]] std::optional<virtual_pi_network<Ntk>>
    run(std::vector<std::vector<mockturtle::node<Ntk>>>& ntk_lvls_new)
    {
        // ToDO: implement border_pis (if there is a choice ush pis to the borders (first or last rank))
        const bool border_pis = true;

        std::unordered_map<typename Ntk::node, int> po_counts;
        ntk.foreach_po([&po_counts](auto po) { po_counts[po]++; });
        std::vector<typename Ntk::node> pos{};
        pos.reserve(ntk.num_pos());
        for (const auto& kv : po_counts)
        {
            pos.push_back(kv.first);
        }

        // Randomize the PO order
        if (ps.random_output_order)
        {
            // Generate a random engine
            static std::mt19937_64 generator(std::random_device{}());
            // Shuffle the pos vector
            std::shuffle(pos.begin(), pos.end(), generator);
        }

        // save the nodes of the next level
        std::vector<mockturtle::node<Ntk>> v_level{};
        v_level.reserve(pos.size());
        // Process the first level
        for (const auto& po : pos)
        {
            // Recalculate the levels to start from the pos
            fis.clear();
            compute_slice_delays(po, border_pis);
            v_level.push_back(po);
        }

        ntk_lvls.push_back(v_level);
        v_level.clear();

        // Try to push PIs to the beginning or end of the vector, since they have to be propagated with buffers
        // until the last levels this should only happen if border_pis == true
        compute_node_order_next_level(v_level);

        bool f_final_level = check_final_level(v_level);

        int lvl_it = 0;
        // Process all other levels
        while (!v_level.empty() && !f_final_level)
        {
            // Push the level to the network
            ntk_lvls.push_back(v_level);
            lvl_pairs.clear();
            // Store the nodes of the next level
            for (const auto& cur_node : v_level)
            {
                fis.clear();
                // There is one slice in the H-Graph for each node in the level
                compute_slice_delays(cur_node, border_pis);
            }
            // Clear before starting computations on the next level
            v_level.clear();
            // Compute the next level
            compute_node_order_next_level(v_level);
            // Check if we are at the final level
            f_final_level = check_final_level(v_level);
            ++lvl_it;
        }
        // Push the final level (PIs)
        if (f_final_level)
        {
            ntk_lvls.push_back(v_level);
        }

        // create virtual pi network
        return virtual_pi_network(ntk, ntk_lvls, ntk_lvls_new);
    }

  private:
    /*
     * The input network.
     */
    Ntk ntk{};
    /*
     * The currently node_pairs used in the current level.
     */
    std::vector<std::vector<node_pair<Ntk>>> lvl_pairs{};
    /*
     * The fanin nodes.
     */
    std::vector<mockturtle::node<Ntk>> fis{};
    /*
     * The network stored as levels.
     */
    std::vector<std::vector<mockturtle::node<Ntk>>> ntk_lvls{};
    /*
     * The stats of the node_duplication class.
     */
    node_duplication_planarization_params ps{};
};

}  // namespace detail

/**
 * Implements a planarization mechanism for networks using a H-Graph strategy for node duplication.
 *
 * The planarization achieved by this function solves the Node Duplication Crossing Minimization (NDCE) problem by
 * finding the shortest x-y path in the H-graph for every level in the network. An H-graph describes edge relations
 * between two levels in a network, with one level assumed as fixed, starting at the Primary Outputs (POs). By finding
 * the shortest path from the source (x) to the sink (y) in this H-graph, an optimal solution for the NDCE problem is
 * found. The function constructs an H-graph that captures edge relations between two levels within the graph and
 * computes the shortest x-y paths on the H-graph, traversing from the POs towards the Primary Inputs (PIs).
 *
 * @return A view of the planarized virtual_pi_network created in the format of extended_rank_view.
 *
 * @tparam NtkDest Destination network type.
 * @tparam NtkSrc Source network type.
 * @param ntk_src Source network to be utilized for the planarization.
 * @param ps Node duplication parameters used in the computation.
 *
 * @throws std::runtime_error if input network not balanced, if no node combinations are found or if
 * the created network is non-planar.
 *
 */
template <typename NtkSrc>
[[nodiscard]] extended_rank_view<virtual_pi_network<technology_network>>
node_duplication_planarization(const NtkSrc& ntk_src, const node_duplication_planarization_params& ps = {})
{
    static_assert(mockturtle::is_network_type_v<NtkSrc>, "NtkSrc is not a network type");

    // check the network to be a technology network (is there a better check/trait?)
    static_assert(mockturtle::has_create_le_v<NtkSrc>, "T must be of type const ExpectedType");

    assert(is_balanced(ntk_src) && "Networks have to be balanced for this duplication");

    detail::node_duplication_planarization_impl p{ntk_src, ps};

    std::vector<std::vector<mockturtle::node<technology_network>>> ntk_lvls_new;

    auto result_ntk = p.run(ntk_lvls_new);

    if (!result_ntk)
    {
        std::cout << "The network is to wide to be processed by the planarization algorithm" << std::endl;
        return extended_rank_view<virtual_pi_network<NtkSrc>>(virtual_pi_network<NtkSrc>(ntk_src));
    }

    std::reverse(ntk_lvls_new.begin(), ntk_lvls_new.end());

    auto result = extended_rank_view(*result_ntk, ntk_lvls_new);

    if (!check_planarity(result))
    {
        throw std::runtime_error("Error: Network should be planar");
    }

    return result;
}

}  // namespace fiction

#endif  // FICTION_NODE_DUPLICATION_PLANARIZATION_HPP
