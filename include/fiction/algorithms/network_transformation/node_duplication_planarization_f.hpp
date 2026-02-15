//
// Created by benjamin on 2/11/26.
//

#ifndef FICTION_NODE_DUPLCATION_PLANARIZATION_F_HPP
#define FICTION_NODE_DUPLCATION_PLANARIZATION_F_HPP

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
     * The output order used. Defaults to KEEP_PO_ORDER.
     */
    output_order po_order = output_order::KEEP_PO_ORDER;
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
        virtual_pi_network<Ntk> duplicated_network{};
        std::unordered_map<mockturtle::node<Ntk>, mockturtle::signal<Ntk>> old2new{};

        // create_pis correctly for equivalence
        // this makes sure old2new[2] = 2, ol2new[3] = 3, ...
        ntk.foreach_pi_unranked([this, &old2new, &duplicated_network](const auto& pi)
        {
            if constexpr(has_is_real_pi_v<Ntk>)
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
            const auto& pi = ntk_lvls[0][i];
            const auto pi_mapped =dupl2old[pi];
            assert(old2new_fis[pi].size() == 0);
            if (!pi_created[dupl2old[pi]])
            {
                pi_created[dupl2old[pi]] = true;
                old2new[pi] = duplicated_network.make_signal(dupl2old[pi]);
                pi_ranks[i] = old2new[pi];
                // duplicated_network.on_add(new_sig);
            }
            else
            {
                const auto pi_create = dupl2old[pi];
                const auto new_sig = duplicated_network.create_virtual_pi(dupl2old[pi]);
                old2new[pi] = new_sig;
                pi_ranks[i] = old2new[pi];
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
                // if i = 1, the children are PIs and need to be mapped using pi2new
                for (auto& child : children)
                {
                    child = old2new[child];
                }
                // only fill old2new for the last level to track POs
                const auto new_sig = duplicated_network.create_node(children, ntk.node_function(dupl2old[nd]));
                old2new[nd] = new_sig;
                // duplicated_network.on_add(new_sig);
            }
        }

        // add primary outputs to finalize the network
        ntk.foreach_po(
            [this, &duplicated_network, &old2new](const auto& po)
            {
                const auto tgt_signal = old2new[ntk.get_node(po)];
                const auto tgt_po     = ntk.is_complemented(po) ? duplicated_network.create_not(tgt_signal) : tgt_signal;

                duplicated_network.create_po(tgt_po);
            });

        duplicated_network.update_ranks();

        duplicated_network.set_ranks(0, pi_ranks);

        // fiction::debug::write_dot_network(duplicated_network, "duplicated_network");

        return duplicated_network;
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

        // Process all other levels
        while (!next_level.empty() && !f_final_level)
        {
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
            // Compute the next level
            next_level = compute_node_order();
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

    // check for planarity
    mincross_stats  st_min{};
    mincross_params p_min{};
    p_min.optimize = false;

    mincross(result, p_min, &st_min);  // counts crossings
    if (st_min.num_crossings != 0)
    {
        throw std::runtime_error("Planarization failed: resulting network is not planar");
    }

    return result;
}

}  // namespace fiction
#endif  // FICTION_NODE_DUPLCATION_PLANARIZATION_F_HPP
