//
// Created by benjamin on 2/24/26.
//

#ifndef FICTION_PLANAR_FANOUT_SUBSTITUTION_HPP
#define FICTION_PLANAR_FANOUT_SUBSTITUTION_HPP

#include "fiction/algorithms/network_transformation/crossing_gate_planarization.hpp"
#include "fiction/algorithms/network_transformation/network_conversion.hpp"
#include "fiction/algorithms/network_transformation/remove_buffer.hpp"
#include "fiction/traits.hpp"

#include <mockturtle/traits.hpp>
#include <mockturtle/utils/node_map.hpp>
#include <mockturtle/views/topo_view.hpp>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <queue>
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
 * Parameters for the fanout substitution algorithm.
 */
struct planar_fanout_substitution_params
{
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

template <typename Ntk>
class planar_fanout_substitution_impl
{
  public:
    planar_fanout_substitution_impl(const Ntk& src, const planar_fanout_substitution_params p) :
            ntk{src},
            fanout_ntk{src},
            available_fanouts{fanout_ntk},
            ps{p}
    {}

    uint32_t fanout_tree_depth(uint32_t fanouts)
    {
        if (fanouts <= 1u)
        {
            return 0u;
        }

        if (ps.degree <= 1u)
        {
            // cannot branch; chain required
            return fanouts - 1u;
        }

        if (ps.degree == 2u)
        {
            // exact integer solution:
            // ceil(log2(fanouts)) == bit_width(fanouts - 1)
            return std::bit_width(fanouts - 1u);
        }

        // general case using logarithm
        const double f{static_cast<double>(fanouts)};
        const double d{static_cast<double>(ps.degree)};

        return static_cast<uint32_t>(std::ceil(std::log(f) / std::log(d)));
    }

    Ntk run()
    {
        using node = mockturtle::node<Ntk>;
        // initialize a network copy
        auto init     = initialize_copy_virtual_pi_network<Ntk>(fanout_ntk);
        auto ntk_dest = init.first;
        auto old2new  = init.second;

        // === process per rank (edges are between rank r-1 and r) ===
        for (uint32_t r = 0u; r < fanout_ntk.depth() + 1; ++r)
        {
            uint32_t max_tree_depth = 0u;
            // 1. iterate through all nodes in the level and collect the buffer tree depth
            ntk.foreach_node_in_rank(r,
                                     [&](auto const& n)
                                     {
                                         if (!ntk.is_fanout(n))
                                         {
                                             max_tree_depth = std::max(max_tree_depth, fanout_tree_depth(fanout_ntk.fanout_size(n)));
                                         }
                                     });
            // std::cout << "[i] maximum fanout tree depth in rank " << r << ": " << max_tree_depth << "\n";

            auto simple_buf_chain = [&](mockturtle::signal<Ntk> s, uint32_t n)
            {
                for (uint32_t i = 0; i < n; ++i)
                {
                    s = ntk_dest.create_buf(s);
                }
                return s;
            };

            // 2. create node by node the buffering or fanout tree for this depth
            // === now copy logic nodes in this rank ===
            ntk.foreach_node_in_rank(r,
                                     [&](auto const& n)
                                     {
                                         if (ntk.is_constant(n))
                                         {
                                             return;
                                         }
                                         if (ntk.is_ci(n))
                                         {
                                             if (fanout_ntk.fanout_size(n) > ps.threshold && !fanout_ntk.is_fanout(n))
                                             {
                                                 const auto tree_depth_diff =
                                                     max_tree_depth - fanout_tree_depth(fanout_ntk.fanout_size(n));
                                                 if (tree_depth_diff != 0u)
                                                 {
                                                     old2new[n] = simple_buf_chain(old2new[n], tree_depth_diff);
                                                 }
                                                 generate_fanout_tree(ntk_dest, n, old2new);
                                             }
                                             else
                                             {
                                                 old2new[n] = simple_buf_chain(old2new[n], max_tree_depth);
                                             }
                                             return;
                                         }

                                         std::vector<typename Ntk::signal> children{};

                                         ntk.foreach_fanin(n,
                                                           [&](auto const& f)
                                                           {
                                                               auto fn = ntk.get_node(f);

                                                               auto child = old2new[fn];

                                                               // constants do not need fanout trees
                                                               if (!ntk.is_constant(fn))
                                                               {
                                                                   child = get_fanout(ntk_dest, fn, child);
                                                               }

                                                               children.push_back(child);
                                                           });

                                         // clone the node with new children according to its depth
                                         old2new[n] = ntk_dest.clone_node(ntk, n, children);

                                         // generate the fanout tree for n
                                         if (fanout_ntk.fanout_size(n) > ps.threshold && !fanout_ntk.is_fanout(n))
                                         {
                                             const auto tree_depth_diff =
                                                 max_tree_depth - fanout_tree_depth(fanout_ntk.fanout_size(n));
                                             if (tree_depth_diff != 0u)
                                             {
                                                 old2new[n] = simple_buf_chain(old2new[n], tree_depth_diff);
                                             }
                                             generate_fanout_tree(ntk_dest, n, old2new);
                                         }
                                         else if (max_tree_depth > 0u)
                                         {
                                             old2new[n] = simple_buf_chain(old2new[n], max_tree_depth);
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

        const auto pi_ranks = fanout_ntk.get_ranks(0);
        ntk_dest.set_ranks(0, pi_ranks);

        return ntk_dest;
    }

  private:
    /**
     * Input network.
     */
    Ntk ntk;
    /**
     * Fanout view of the network.
     */
    mockturtle::fanout_view<Ntk> fanout_ntk;
    /**
     * Type alias for mapping original nodes to new signals.
     */
    using old2new_map = mockturtle::node_map<mockturtle::signal<Ntk>, Ntk>;
    /**
     * Type alias for mapping each node to a queue of buffer outputs.
     */
    using old2new_queue_map = mockturtle::node_map<std::queue<mockturtle::signal<Ntk>>, mockturtle::topo_view<Ntk>>;
    /**
     * Queue map of available fanouts.
     */
    old2new_queue_map available_fanouts;
    /**
     * Parameters controlling how fanout substitution is performed.
     */
    const planar_fanout_substitution_params ps;

    void generate_fanout_tree(Ntk& substituted, const mockturtle::node<Ntk>& n, const old2new_map& old2new)
    {
        // skip fanout tree generation if n is a proper fanout node
        if constexpr (has_is_fanout_v<mockturtle::fanout_view<Ntk>>)
        {
            if (fanout_ntk.is_fanout(n) && fanout_ntk.fanout_size(n) <= ps.degree)
            {
                return;
            }
        }

        auto num_fanouts = static_cast<uint32_t>(
            std::ceil(static_cast<double>(std::max(
                          static_cast<int32_t>(fanout_ntk.fanout_size(n)) - static_cast<int32_t>(ps.threshold), 0)) /
                      static_cast<double>(std::max(static_cast<int32_t>(ps.degree) - 1, 1))));

        auto child = old2new[n];

        if (num_fanouts == 0)
        {
            return;
        }

        generate_breadth_tree(substituted, n, child, num_fanouts);
    }

    mockturtle::signal<Ntk> get_fanout(const Ntk& substituted, const mockturtle::node<Ntk>& n,
                                       mockturtle::signal<Ntk>& child)
    {
        if (substituted.fanout_size(substituted.get_node(child)) >= ps.threshold)
        {
            if (auto fanouts = available_fanouts[n]; !fanouts.empty())
            {
                // find non-overfull fanout node
                while (true)
                {
                    child = fanouts.front();
                    if (substituted.fanout_size(substituted.get_node(child)) >= ps.degree)
                    {
                        fanouts.pop();
                    }
                    else
                    {
                        break;
                    }
                }
            }
        }

        return child;
    }

    uint32_t fanout_node_level( uint32_t fanout_index ) const
    {
        const uint32_t d{ ps.degree };

        if ( fanout_index == 0u || d <= 1u )
        {
            return 0u;
        }

        const double x =
            (static_cast<double>( fanout_index ) * static_cast<double>( d - 1u )) + 1.0;

        return static_cast<uint32_t>(
            std::floor( std::log( x ) / std::log( static_cast<double>( d ) ) )
        );
    }

    void generate_breadth_tree(Ntk& substituted, const mockturtle::node<Ntk>& n, mockturtle::signal<Ntk>& child,
                               const uint32_t num_fanouts)
    {
        std::queue<mockturtle::signal<Ntk>> q{{child}};
        std::unordered_map<mockturtle::signal<Ntk>, uint32_t> sig_level{};
        uint32_t max_level = 0u;

        for (auto f = 0ul; f < num_fanouts; ++f)
        {
            child = q.front();
            q.pop();
            child = substituted.create_buf(child);
            const auto level = fanout_node_level(f);
            sig_level[child] = level;
            max_level = std::max(max_level, level);

            for (auto i = 0u; i < ps.degree; ++i)
            {
                q.push(child);
            }
        }

        // buffer to the maximum level (which is maximum plus one level in a breadth tree)
        mockturtle::signal<Ntk> last_sig{};
        while (!q.empty())
        {
            auto sig = q.front();
            if (sig == last_sig)
            {
                q.pop();
                continue;
            }
            if (sig_level[sig] == max_level)
            {
                break;
            }
            q.pop();
            last_sig = sig;
            sig = substituted.create_buf(sig);
            for (auto i = 0u; i < ps.degree; ++i)
            {
                q.push(sig);
            }
        }

        available_fanouts[n] = std::move(q);
    }
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
 * @tparam Ntk Type of the input logic network.
 * @param ntk_src The input logic network.
 * @param ps Parameters.
 * @return A fanout-substituted logic network of type `NtkDest` that is logically equivalent to `ntk_src`.
 */
template <typename Ntk>
Ntk planar_fanout_substitution(const Ntk& ntk_src, planar_fanout_substitution_params ps = {})
{
    static_assert(mockturtle::is_network_type_v<Ntk>, "Ntk is not a network type");

    static_assert(mockturtle::has_is_constant_v<Ntk>, "NtkSrc does not implement the is_constant function");
    static_assert(mockturtle::has_create_pi_v<Ntk>, "NtkDest does not implement the create_pi function");
    static_assert(mockturtle::has_create_not_v<Ntk>, "NtkDest does not implement the create_not function");
    static_assert(mockturtle::has_create_po_v<Ntk>, "NtkDest does not implement the create_po function");
    static_assert(mockturtle::has_create_buf_v<Ntk>, "NtkDest does not implement the create_buf function");
    static_assert(mockturtle::has_clone_node_v<Ntk>, "NtkDest does not implement the clone_node function");
    static_assert(mockturtle::has_fanout_size_v<Ntk>, "NtkDest does not implement the fanout_size function");
    static_assert(mockturtle::has_foreach_gate_v<Ntk>, "NtkDest does not implement the foreach_gate function");
    static_assert(mockturtle::has_foreach_fanin_v<Ntk>, "NtkDest does not implement the foreach_fanin function");
    static_assert(mockturtle::has_foreach_po_v<Ntk>, "NtkDest does not implement the foreach_po function");
    static_assert(mockturtle::has_rank_position_v<Ntk>, "NtkDest does not implement the has_rank_position function");

    detail::planar_fanout_substitution_impl<Ntk> p{ntk_src, ps};

    auto result = p.run();

    return result;
}

}  // namespace fiction

#endif  // FICTION_PLANAR_FANOUT_SUBSTITUTION_HPP
