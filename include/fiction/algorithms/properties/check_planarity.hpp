//
// Created by benjamin on 17.07.24.
//

#ifndef FICTION_CHECK_PLANARITY_HPP
#define FICTION_CHECK_PLANARITY_HPP

#include "fiction/algorithms/network_transformation/network_balancing.hpp"

#include <mockturtle/traits.hpp>

#include <algorithm>
#include <cstdint>
#include <stdexcept>

namespace fiction
{

template <typename Ntk>
class check_planarity_impl
{
  public:
    check_planarity_impl(const Ntk& ntk) : ntk(ntk) {}

    /**
     * @brief Checks if a given network is planar.
     *
     * This function checks if the network represented by the variable `ntk` is planar.
     * The network is planar if for any edge with starting point m and endpoint n (represented by the node ranks), there
     * is never another edge with starting point m_ > m and endpoint n_ < n, or vice versa. When iterating through
     * the ranks of one level, the endpoints are always increasing. Therefore, only the starting points need to be
     * checked. Thus, the highest connected starting point in the fan-in gives a border m_max for every subsequent edge.
     *
     * @return `true` if the network is planar, `false` otherwise.
     */
    bool run()
    {
        bool return_false = false;
        for (uint32_t r = 1; r < ntk.depth() + 1; r++)
        {
            uint32_t bound = 0;
            ntk.foreach_node_in_rank(r,
                                     [this, &bound, &return_false](const auto& n)
                                     {
                                         uint32_t new_bound = bound;
                                         ntk.foreach_fanin(n,
                                                           [this, &bound, &new_bound, &return_false](const auto& fi)
                                                           {
                                                               const auto fi_n = ntk.get_node(fi);
                                                               if (ntk.rank_position(fi_n) < bound)
                                                               {
                                                                   return_false = true;
                                                               }
                                                               new_bound = std::max(new_bound, ntk.rank_position(fi_n));
                                                           });
                                         if (return_false)
                                             return;
                                         bound = new_bound;
                                     });
            if (return_false)
                return false;
        }
        return true;
    }

  private:
    const Ntk ntk;
};

/**
 * Checks if a logic network is planar. To perform this check, the network must have ranks assigned.
 *
 * If the network is not balanced, an exception is thrown. To balance the network, insert buffers to divide multi-level
 * edges.
 *
 * @tparam Ntk Logic network type.
 * @param ntk The logic network to check for planarity.
 * @return `true` if the network is planar, `false` otherwise.
 * @throw std::runtime_error if the network is not balanced.
 */
template <typename Ntk>
bool check_planarity(const Ntk& ntk)
{
    static_assert(mockturtle::has_get_node_v<Ntk>, "Ntk does not have get_node trait");
    static_assert(mockturtle::has_foreach_fanin_v<Ntk>, "Ntk does not have foreach_fanin trait");
    static_assert(mockturtle::has_depth_v<Ntk>, "Ntk does not have depth trait");
    static_assert(mockturtle::has_rank_position_v<Ntk>, "Ntk does not have rank_position trait");
    static_assert(mockturtle::has_foreach_node_in_rank_v<Ntk>, "Ntk does not have foreach_node_in_rank trait");

    if (!is_balanced(ntk))
    {
        throw std::runtime_error("Network must be balanced");
    }

    check_planarity_impl<Ntk> p{ntk};

    const auto result = p.run();

    return result;
}

}  // namespace fiction

#endif  // FICTION_CHECK_PLANARITY_HPP
