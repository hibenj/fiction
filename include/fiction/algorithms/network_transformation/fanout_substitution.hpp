//
// Created by marcel on 31.05.21.
//

#ifndef FICTION_FANOUT_SUBSTITUTION_HPP
#define FICTION_FANOUT_SUBSTITUTION_HPP

#include "fiction/algorithms/network_transformation/network_conversion.hpp"
#include "fiction/traits.hpp"

#include <mockturtle/algorithms/cleanup.hpp>
#include <mockturtle/traits.hpp>
#include <mockturtle/utils/node_map.hpp>
#include <mockturtle/views/topo_view.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <deque>
#include <functional>
#include <optional>
#include <queue>
#include <random>
#include <utility>
#include <vector>

#if (PROGRESS_BARS)
#include <mockturtle/utils/progress_bar.hpp>
#endif

namespace fiction
{

/**
 * Parameters for the fanout substitution algorithm.
 */
struct fanout_substitution_params
{
    /**
     * Breadth-first vs. depth-first fanout-tree substitution strategies.
     */
    enum substitution_strategy
    {
        /**
         * Breadth-first substitution. Creates balanced fanout trees.
         */
        BREADTH,
        /**
         * Depth-first substitution. Creates fanout trees with one deep branch.
         */
        DEPTH,
        /**
         * Random substitution. Inserts fanout buffers at random positions in the fanout tree.
         */
        RANDOM
    };

    /**
     * Substitution strategy of high-degree fanout networks (depth-first vs. breadth-first).
     */
    substitution_strategy strategy = BREADTH;
    /**
     * Maximum output degree of each fan-out node.
     */
    uint32_t degree = 2ul;
    /**
     * Maximum number of outputs any gate is allowed to have before substitution applies.
     */
    uint32_t threshold = 1ul;
    /**
     * Seed used for random substitution, generated randomly if not specified.
     */
    std::optional<uint32_t> seed = std::nullopt;
};

namespace detail
{

/**
 * A lightweight container that groups together the two objects required for random fan-out selection and only lives
 * when `strategy == RANDOM`.
 */
struct rng_state
{
    /**
     * Random number generation engine that is seeded once in the constructor of `fanout_substitution_impl` and then
     * reused for all random draws during fanout substitution.
     */
    std::mt19937 gen;
    /**
     * Uniform distribution whose parameter range is re-initialised (`dist.param({0, upper})`) every time the pool of
     * candidate signals changes size.
     */
    std::uniform_int_distribution<size_t> dist{0, 0};
    /**
     * Default constructor.
     *
     * @param seed The seed for the random number generator.
     */
    explicit rng_state(const uint32_t seed) noexcept : gen{seed} {}
};

template <typename NtkDest, typename NtkSrc>
class fanout_substitution_impl
{
  public:
    fanout_substitution_impl(const NtkSrc& src, const fanout_substitution_params p) :
            ntk_topo{convert_network<NtkDest>(src)},
            available_fanouts{ntk_topo},
            ps{p}
    {
        if (ps.strategy == fanout_substitution_params::RANDOM)
        {
            rng.emplace(ps.seed.value_or(std::random_device{}()));
        }
    }

    NtkDest run()
    {
        // initialize a network copy
        auto init = mockturtle::initialize_copy_network<NtkDest>(ntk_topo);

        auto substituted = init.first;
        auto old2new     = init.second;

        ntk_topo.foreach_pi([this, &substituted, &old2new](const auto& pi)
                            { generate_fanout_tree(substituted, pi, old2new); });

#if (PROGRESS_BARS)
        // initialize a progress bar
        mockturtle::progress_bar bar{static_cast<uint32_t>(ntk_topo.num_gates()), "[i] fanout substitution: |{0}|"};
#endif

        ntk_topo.foreach_gate(
            [&, this](const auto& n, [[maybe_unused]] auto i)
            {
                // gather children, but substitute fanouts where applicable
                std::vector<mockturtle::signal<mockturtle::topo_view<NtkDest>>> children{};

                ntk_topo.foreach_fanin(n,
                                       [this, &old2new, &children, &substituted](const auto& f)
                                       {
                                           const auto fn = ntk_topo.get_node(f);

                                           auto child = old2new[fn];

                                           // constants do not need fanout trees
                                           if (!ntk_topo.is_constant(fn))
                                           {
                                               child = get_fanout(substituted, fn, child);
                                           }

                                           children.push_back(child);
                                       });

                // clone the node with new children according to its depth
                old2new[n] = substituted.clone_node(ntk_topo, n, children);

                // generate the fanout tree for n
                generate_fanout_tree(substituted, n, old2new);

#if (PROGRESS_BARS)
                // update progress
                bar(i);
#endif
            });

        // add primary outputs to finalize the network
        ntk_topo.foreach_po(
            [this, &old2new, &substituted](const auto& po)
            {
                const auto po_node    = ntk_topo.get_node(po);
                auto       tgt_signal = old2new[po_node];
                auto       tgt_po     = get_fanout(substituted, po_node, tgt_signal);

                tgt_po = ntk_topo.is_complemented(po) ? substituted.create_not(tgt_signal) : tgt_po;

                substituted.create_po(tgt_po);
            });

        // restore signal names if applicable
        fiction::restore_names(ntk_topo, substituted, old2new);

        return substituted;
    }

  private:
    /**
     * Topological view of the converted network.
     */
    mockturtle::topo_view<NtkDest> ntk_topo;
    /**
     * Type alias for mapping original nodes to new signals.
     */
    using old2new_map = mockturtle::node_map<mockturtle::signal<NtkDest>, mockturtle::topo_view<NtkDest>>;
    /**
     * Type alias for mapping each node to a queue of buffer outputs.
     */
    using old2new_queue_map =
        mockturtle::node_map<std::queue<mockturtle::signal<NtkDest>>, mockturtle::topo_view<NtkDest>>;
    /**
     * Queue map of available fanouts.
     */
    old2new_queue_map available_fanouts;
    /**
     * Parameters controlling how fanout substitution is performed.
     */
    const fanout_substitution_params ps;
    /**
     * Optional helper struct holding the RNG and its distribution.
     */
    std::optional<rng_state> rng;

    void generate_fanout_tree(NtkDest& substituted, const mockturtle::node<NtkSrc>& n, const old2new_map& old2new)
    {
        // skip fanout tree generation if n is a proper fanout node
        if constexpr (has_is_fanout_v<NtkDest>)
        {
            if (ntk_topo.is_fanout(n) && ntk_topo.fanout_size(n) <= ps.degree)
            {
                return;
            }
        }

        auto num_fanouts = static_cast<uint32_t>(
            std::ceil(static_cast<double>(std::max(
                          static_cast<int32_t>(ntk_topo.fanout_size(n)) - static_cast<int32_t>(ps.threshold), 0)) /
                      static_cast<double>(std::max(static_cast<int32_t>(ps.degree) - 1, 1))));

        auto child = old2new[n];

        if (num_fanouts == 0)
        {
            return;
        }

        switch (ps.strategy)
        {
            case fanout_substitution_params::substitution_strategy::DEPTH:
            {
                generate_depth_tree(substituted, n, child, num_fanouts);
                break;
            }

            case fanout_substitution_params::substitution_strategy::BREADTH:
            {
                generate_breadth_tree(substituted, n, child, num_fanouts);
                break;
            }

            case fanout_substitution_params::substitution_strategy::RANDOM:
            {
                generate_random_tree(substituted, n, child, num_fanouts);
                break;
            }
        }
    }

    mockturtle::signal<NtkDest> get_fanout(const NtkDest& substituted, const mockturtle::node<NtkSrc>& n,
                                           mockturtle::signal<NtkDest>& child)
    {
        if (substituted.fanout_size(child) >= ps.threshold)
        {
            if (auto fanouts = available_fanouts[n]; !fanouts.empty())
            {
                // find non-overfull fanout node
                do {
                    child = fanouts.front();
                    if (substituted.fanout_size(child) >= ps.degree)
                    {
                        fanouts.pop();
                    }
                    else
                    {
                        break;
                    }
                } while (true);
            }
        }

        return child;
    }
    /**
     * DEPTH-FIRST strategy: create a chain of buffers.
     *
     * @param substituted  The partially constructed destination network (NtkDest)
     * @param n            The current node in the topological view (NtkSrc)
     * @param child        The signal in substituted representing the output of node n.
     * @param num_fanouts  Number of buffers to insert (chain length)
     */
    void generate_depth_tree(NtkDest& substituted, const mockturtle::node<NtkSrc>& n,
                             mockturtle::signal<NtkDest>& child, const uint32_t num_fanouts)
    {
        std::queue<mockturtle::signal<NtkDest>> q{};
        for (auto i = 0u; i < num_fanouts; ++i)
        {
            child = substituted.create_buf(child);
            q.push(child);
        }
        available_fanouts[n] = std::move(q);
    }
    /**
     * BREADTH-FIRST strategy: expand buffers level by level to create balanced fanout trees.
     *
     * @param substituted  The partially constructed destination network (NtkDest)
     * @param n            The current node in the topological view (NtkSrc)
     * @param child        The signal in substituted representing the output of node n.
     * @param num_fanouts  Number of buffers to insert (chain length)
     */
    void generate_breadth_tree(NtkDest& substituted, const mockturtle::node<NtkSrc>& n,
                               mockturtle::signal<NtkDest>& child, const uint32_t num_fanouts)
    {
        std::queue<mockturtle::signal<NtkDest>> q{{child}};

        for (auto f = 0ul; f < num_fanouts; ++f)
        {
            child = q.front();
            q.pop();
            child = substituted.create_buf(child);

            for (auto i = 0u; i < ps.degree; ++i)
            {
                q.push(child);
            }
        }
        available_fanouts[n] = std::move(q);
    }
    /**
     * RANDOM strategy: insert buffers at randomly chosen positions in the expanding tree.
     *
     * @param substituted  The partially constructed destination network (NtkDest)
     * @param n            The current node in the topological view (NtkSrc)
     * @param child        The signal in substituted representing the output of node n.
     * @param num_fanouts  Number of buffers to insert (chain length)
     */
    void generate_random_tree(NtkDest& substituted, const mockturtle::node<NtkSrc>& n,
                              mockturtle::signal<NtkDest>& child, const uint32_t num_fanouts)
    {
        auto& gen  = rng->gen;
        auto& dist = rng->dist;
        // maintain a vector of available fanout nodes and randomly select one
        std::vector<mockturtle::signal<NtkDest>> available_vec{child};
        dist.param(typename std::uniform_int_distribution<std::size_t>::param_type(0, available_vec.size() - 1));

        for (auto f = 0u; f < num_fanouts; ++f)
        {
            // get a random index
            const auto index    = dist(gen);
            const auto selected = available_vec[index];

            // remove the selected element using swap-and-pop
            std::swap(available_vec[index], available_vec.back());
            available_vec.pop_back();

            const auto new_buf = substituted.create_buf(selected);

            // add 'ps.degree' copies of the new buffer into available_vec
            for (auto i = 0u; i < ps.degree; ++i)
            {
                available_vec.push_back(new_buf);
            }

            if (!available_vec.empty())
            {
                dist.param(
                    typename std::uniform_int_distribution<std::size_t>::param_type(0, available_vec.size() - 1));
            }
        }
        // transfer the available nodes to a queue for later use in get_fanout
        std::queue<mockturtle::signal<NtkDest>> q{};
        for (auto const& sig : available_vec)
        {
            q.push(sig);
        }
        available_fanouts[n] = std::move(q);
    }
};

template <typename Ntk>
class is_fanout_substituted_impl
{
  public:
    is_fanout_substituted_impl(const Ntk& src, fanout_substitution_params p) : ntk{src}, ps{p} {}

    bool run()
    {
        ntk.foreach_node(
            [this](const auto& n)
            {
                // skip constants
                if (ntk.is_constant(n))
                {
                    return substituted;
                }

                // check degree of fanout nodes
                if constexpr (fiction::has_is_fanout_v<Ntk>)
                {
                    if (ntk.is_fanout(n))
                    {
                        if (ntk.fanout_size(n) > ps.degree)
                        {
                            substituted = false;
                        }

                        return substituted;
                    }
                }
                // check threshold of non-fanout nodes
                if (ntk.fanout_size(n) > ps.threshold)
                {
                    substituted = false;
                }

                return substituted;
            });

        return substituted;
    }

  private:
    const Ntk& ntk;

    const fanout_substitution_params ps;

    bool substituted = true;
};

}  // namespace detail

/**
 * Substitutes high-output degrees in a logic network with fanout nodes that compute the identity function. For this
 * purpose, `create_buf` is utilized. Therefore, `NtkDest` should support identity nodes. If it does not, no new nodes
 * will in fact be created. In either case, the returned network will be logically equivalent to the input one.
 *
 * The process is rather naive with two possible strategies to pick from: breath-first and depth-first. The former
 * creates partially balanced fanout trees while the latter leads to fanout chains. Further parameterization includes
 * thresholds for the maximum number of output each node and fanout is allowed to have.
 *
 * The returned network is newly created from scratch because its type `NtkDest` may differ from `NtkSrc`.
 *
 * @note The physical design algorithms natively provided in fiction do not require their input networks to be
 * fanout-substituted. If that is necessary, they will do it themselves. Providing already substituted networks does
 * however allow for the control over maximum output degrees.
 *
 * @tparam NtkDest Type of the returned logic network.
 * @tparam NtkSrc Type of the input logic network.
 * @param ntk_src The input logic network.
 * @param ps Parameters.
 * @return A fanout-substituted logic network of type `NtkDest` that is logically equivalent to `ntk_src`.
 */
template <typename NtkDest, typename NtkSrc>
NtkDest fanout_substitution(const NtkSrc& ntk_src, fanout_substitution_params ps = {})
{
    static_assert(mockturtle::is_network_type_v<NtkSrc>, "NtkSrc is not a network type");
    static_assert(mockturtle::is_network_type_v<NtkDest>, "NtkDest is not a network type");

    static_assert(mockturtle::has_is_constant_v<NtkDest>, "NtkSrc does not implement the is_constant function");
    static_assert(mockturtle::has_create_pi_v<NtkDest>, "NtkDest does not implement the create_pi function");
    static_assert(mockturtle::has_create_not_v<NtkDest>, "NtkDest does not implement the create_not function");
    static_assert(mockturtle::has_create_po_v<NtkDest>, "NtkDest does not implement the create_po function");
    static_assert(mockturtle::has_create_buf_v<NtkDest>, "NtkDest does not implement the create_buf function");
    static_assert(mockturtle::has_clone_node_v<NtkDest>, "NtkDest does not implement the clone_node function");
    static_assert(mockturtle::has_fanout_size_v<NtkDest>, "NtkDest does not implement the fanout_size function");
    static_assert(mockturtle::has_foreach_gate_v<NtkDest>, "NtkDest does not implement the foreach_gate function");
    static_assert(mockturtle::has_foreach_fanin_v<NtkDest>, "NtkDest does not implement the foreach_fanin function");
    static_assert(mockturtle::has_foreach_po_v<NtkDest>, "NtkDest does not implement the foreach_po function");

    detail::fanout_substitution_impl<NtkDest, NtkSrc> p{ntk_src, ps};

    auto result = p.run();

    return result;
}
/**
 * Checks if a logic network is properly fanout-substituted with regard to the provided parameters, i.e., if no node
 * exceeds the specified fanout limits.
 *
 * @tparam Ntk Logic network type.
 * @param ntk The logic network to check.
 * @param ps Parameters.
 * @return `true` iff `ntk` is properly fanout-substituted with regard to `ps`.
 */
template <typename Ntk>
bool is_fanout_substituted(const Ntk& ntk, fanout_substitution_params ps = {}) noexcept
{
    static_assert(mockturtle::is_network_type_v<Ntk>, "NtkSrc is not a network type");
    static_assert(mockturtle::has_foreach_gate_v<Ntk>, "Ntk does not implement the foreach_gate function");
    static_assert(mockturtle::has_fanout_size_v<Ntk>, "Ntk does not implement the fanout_size function");

    detail::is_fanout_substituted_impl<Ntk> p{ntk, ps};

    auto result = p.run();

    return result;
}

}  // namespace fiction

#endif  // FICTION_FANOUT_SUBSTITUTION_HPP
