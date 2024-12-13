//
// Created by benjamin on 13.04.23.
//

#ifndef FICTION_INVERTER_SUBSTITUTION_HPP
#define FICTION_INVERTER_SUBSTITUTION_HPP

#include "fiction/algorithms/properties/count_gate_types.hpp"
#include "fiction/traits.hpp"
#include "fiction/utils/name_utils.hpp"

#include <mockturtle/traits.hpp>
#include <mockturtle/utils/node_map.hpp>
#include <mockturtle/views/fanout_view.hpp>
#include <mockturtle/views/topo_view.hpp>

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>

#if (PROGRESS_BARS)
#include <mockturtle/utils/progress_bar.hpp>
#endif

namespace fiction
{

namespace detail
{

/**
 * This function connects gates that aren't affected by the inverter substitution. It means that all the gates without
 * any fan-ins or fan-outs impacted by the inverter substitution retain their functionality and are subsequently
 * connected to their children.
 *
 * @tparam Ntk Type of the input logic network.
 * @tparam NtkDest Type of the returned logic network.
 * @param ntk Input network.
 * @param ntk_dest Output network.
 * @param old2new `node_map` to assign the nodes of the old network to the new network.
 * @param children Children of the current gate.
 * @param g Currently viewed gate.
 * @return 'true' iff the assignment was successful.
 */
template <typename Ntk, typename NtkDest>
bool connect_children_to_gates_unaffected(const Ntk& ntk, NtkDest& ntk_dest,
                                          mockturtle::node_map<mockturtle::signal<Ntk>, Ntk>& old2new,
                                          const mockturtle::node<Ntk>&                        g,
                                          const std::vector<typename Ntk::signal>&            children)
{
    if constexpr (mockturtle::has_is_and_v<Ntk> && mockturtle::has_create_and_v<NtkDest>)
    {
        if (ntk.is_and(g))
        {
            old2new[g] = ntk_dest.create_and(children[0], children[1]);
            return true;  // keep looping
        }
    }
    if constexpr (mockturtle::has_is_or_v<Ntk> && mockturtle::has_create_or_v<NtkDest>)
    {
        if (ntk.is_or(g))
        {
            old2new[g] = ntk_dest.create_or(children[0], children[1]);
            return true;  // keep looping
        }
    }
    if constexpr (mockturtle::has_is_xor_v<Ntk> && mockturtle::has_create_xor_v<NtkDest>)
    {
        if (ntk.is_xor(g))
        {
            old2new[g] = ntk_dest.create_xor(children[0], children[1]);
            return true;  // keep looping
        }
    }
    if constexpr (mockturtle::has_is_maj_v<Ntk> && mockturtle::has_create_maj_v<NtkDest>)
    {
        if (ntk.is_maj(g))
        {
            old2new[g] = ntk_dest.create_maj(children[0], children[1], children[2]);
            return true;  // keep looping
        }
    }
    if constexpr (mockturtle::has_is_nary_and_v<Ntk> && mockturtle::has_create_nary_and_v<NtkDest>)
    {
        if (ntk.is_nary_and(g))
        {
            old2new[g] = ntk_dest.create_nary_and(children);
            return true;  // keep looping
        }
    }
    if constexpr (mockturtle::has_is_nary_or_v<Ntk> && mockturtle::has_create_nary_or_v<NtkDest>)
    {
        if (ntk.is_nary_or(g))
        {
            old2new[g] = ntk_dest.create_nary_or(children);
            return true;  // keep looping
        }
    }
    if constexpr (mockturtle::has_is_nary_xor_v<Ntk> && mockturtle::has_create_nary_xor_v<NtkDest>)
    {
        if (ntk.is_nary_xor(g))
        {
            old2new[g] = ntk_dest.create_nary_xor(children);
            return true;  // keep looping
        }
    }
    if constexpr (mockturtle::has_is_not_v<Ntk> && mockturtle::has_create_not_v<NtkDest>)
    {
        if (ntk.is_not(g))
        {
            old2new[g] = ntk_dest.create_not(children[0]);
            return true;
        }
    }
    if constexpr (fiction::has_is_buf_v<Ntk> && mockturtle::has_create_buf_v<NtkDest>)
    {
        if (ntk.is_buf(g))
        {
            old2new[g] = ntk_dest.create_buf(children[0]);
            return true;
        }
    }
    if constexpr (mockturtle::has_node_function_v<Ntk> && mockturtle::has_create_node_v<NtkDest>)
    {
        old2new[g] = ntk_dest.create_node(children, ntk.node_function(g));
        return true;  // keep looping
    }
    return false;  // gate type not supported
}

enum class operation_mode : std::uint8_t
{
    FO_ONLY,
    AND_OR_ONLY,
    ALL_NODES
};

template <typename Ntk>
class inverter_substitution_impl
{
  public:
    explicit inverter_substitution_impl(const Ntk& ntk_src, operation_mode substitution_mode) :
            ntk{ntk_src},
            mode{substitution_mode}
    {
        // compute maximum sizes for vectors
        count_gate_types_stats st{};
        count_gate_types(fo_ntk, &st);
        x_inv_fo.reserve(st.num_fanout);
        m_inv_fo.reserve(st.num_fanout);
        fo_nodes.reserve(st.num_fanout);

        // enable premature termination, when no optimizations are possible
        if (substitution_mode == operation_mode::FO_ONLY)
        {
            if (st.num_fanout == 0 || st.num_inv < 2)
            {
                rerun = false;
            }
        }
        if (substitution_mode == operation_mode::AND_OR_ONLY)
        {
            if ((st.num_and2 == 0 && st.num_or2 == 0) || st.num_inv < 2)
            {
                rerun = false;
            }
        }
        if (substitution_mode == operation_mode::ALL_NODES)
        {
            if ((st.num_fanout == 0 || st.num_inv < 2) && ((st.num_and2 == 0 && st.num_or2 == 0) || st.num_inv < 2))
            {
                rerun = false;
            }
        }
    }

    // check if optimizations were made
    [[nodiscard]] bool is_rerun() const
    {
        return rerun;
    }

    Ntk run()
    {
        auto  init     = mockturtle::initialize_copy_network<Ntk>(ntk);
        auto& ntk_dest = init.first;
        auto& old2new  = init.second;

#if (PROGRESS_BARS)
        // initialize a progress bar
        mockturtle::progress_bar bar{static_cast<uint32_t>(ntk.num_gates()), "[i] inverter substitution: |{0}|"};
#endif

        ntk.foreach_gate(
            [&, this](const auto& g, [[maybe_unused]] auto i)
            {
                const auto children = gather_fanin_signals<TopoNtkSrc>(g, old2new);

                for (const auto& pair : delayed_nodes) {
                    const auto& arr = pair.second;
                    if (std::find(arr.begin(), arr.end(), g) != arr.end()) {
                        return true;
                    }
                }

#if (PROGRESS_BARS)
                // update progress
                bar(i);
#endif

                connect_children_to_gates_affected<TopoNtkSrc>(ntk_dest, old2new, g, children);
                if (delayed_nodes.find(g) != delayed_nodes.end())
                {
                    const auto children0 = gather_fanin_signals<TopoNtkSrc>(delayed_nodes[g][0], old2new);
                    connect_children_to_gates_affected<TopoNtkSrc>(ntk_dest, old2new, delayed_nodes[g][0], children0);
                    const auto children1 = gather_fanin_signals<TopoNtkSrc>(delayed_nodes[g][1], old2new);
                    connect_children_to_gates_affected<TopoNtkSrc>(ntk_dest, old2new, delayed_nodes[g][1], children1);
                    const auto children2 = gather_fanin_signals<TopoNtkSrc>(delayed_nodes[g][2], old2new);
                    connect_children_to_gates_affected<TopoNtkSrc>(ntk_dest, old2new, delayed_nodes[g][2], children2);
                }

                return true;  // keep looping
            });

        // create the POs of the network, POs of deleted inverters need to be preserved
        ntk.foreach_po(
            [this, &ntk_dest, &old2new](const auto& po)
            {
                auto tgt_signal = old2new[ntk.get_node(po)];
                auto tgt_po     = ntk.is_complemented(po) ? ntk_dest.create_not(tgt_signal) : tgt_signal;
                if (tgt_po == mockturtle::signal<Ntk>{} && !ntk.is_constant(po))
                {
                    tgt_signal = old2new[ntk.get_node(preserved_po[0])];
                    preserved_po.erase(preserved_po.cbegin());
                    tgt_po = ntk.is_complemented(po) ? ntk_dest.create_not(tgt_signal) : tgt_signal;
                }
                ntk_dest.create_po(tgt_po);
            });

        // restore signal names if applicable
        restore_names(ntk, ntk_dest, old2new);

        // check if any optimizations were made
        if (mode == operation_mode::FO_ONLY)
        {
            if (fo_nodes.empty())
            {
                rerun = false;
            }
        }
        else if (mode == operation_mode::AND_OR_ONLY)
        {
            if (and_or_nodes.empty())
            {
                rerun = false;
            }
        }
        else if (mode == operation_mode::ALL_NODES)
        {
            if (and_or_nodes.empty() && fo_nodes.empty())
            {
                rerun = false;
            }
        }

        return ntk_dest;
    }

  private:
    /**
     * Alias for a topological view of a network.
     */
    using TopoNtkSrc = mockturtle::topo_view<Ntk>;
    /**
     * A topologically ordered input logic network.
     */
    TopoNtkSrc ntk;
    /**
     * This is a fan-out view of the network 'ntk'.
     */
    mockturtle::fanout_view<TopoNtkSrc> fo_ntk{ntk};
    /**
     * A collection of inverter nodes that are set to be deleted.
     */
    std::vector<mockturtle::node<Ntk>> x_inv_fo{};
    /**
     * A collection of inverter nodes that are set to be moved to fan-in position.
     */
    std::vector<mockturtle::node<Ntk>> m_inv_fo{};
    /**
     * These are fan-out nodes where balancing is applied.
     */
    std::vector<mockturtle::node<Ntk>> fo_nodes{};
    /**
     * A collection of inverter nodes that are set to be deleted.
     */
    std::vector<mockturtle::node<Ntk>> x_inv_ao{};
    /**
     * A collection of inverter nodes that are set to be moved to fan-in position.
     */
    std::vector<mockturtle::node<Ntk>> m_inv_ao{};
    /**
     * These are and/or nodes where balancing is applied.
     */
    std::vector<mockturtle::node<Ntk>> and_or_nodes{};
    /**
     * Collection of nodes where the children have not been placed yet.
     */
    std::unordered_map<mockturtle::node<Ntk>, std::array<mockturtle::node<Ntk>, 3>> delayed_nodes{};
    /**
     * Collection of nodes where primary outputs need to be preserved.
     */
    std::vector<mockturtle::node<Ntk>> preserved_po{};
    /**
     * An indicator to check if optimizations can be applied or not.
     */
    bool rerun{true};
    /**
     * The operation mode of inverter substitution.
     * */
    detail::operation_mode mode;

    template <typename NtkSrc>
    auto gather_fanin_signals(const typename Ntk::node&                              n,
                              mockturtle::node_map<mockturtle::signal<Ntk>, NtkSrc>& old2new)
    {
        std::vector<typename Ntk::signal> children{};

        // store affected nodes fo
        if (fo_ntk.is_fanout(n) && (mode == operation_mode::FO_ONLY || mode == operation_mode::ALL_NODES))
        {
            const auto fanout_inv    = fanouts(fo_ntk, n);
            const auto do_substitute = std::all_of(fanout_inv.cbegin(), fanout_inv.cend(),
                                                   [this](const auto& fo_node) { return fo_ntk.is_inv(fo_node); });

            if (do_substitute && fanout_inv.size() > 1)
            {
                x_inv_fo.emplace_back(fanout_inv[1]);
                fo_nodes.emplace_back(n);
                m_inv_fo.emplace_back(fanout_inv[0]);
            }
        }
        // store affected nodes and/or
        if (fo_ntk.is_inv(n) && (mode == operation_mode::AND_OR_ONLY || mode == operation_mode::ALL_NODES))
        {
            const auto inv_fanout = fanouts(fo_ntk, n);
            assert(inv_fanout.size() == 1);
            if ((fo_ntk.is_and(inv_fanout[0]) || fo_ntk.is_or(inv_fanout[0])) &&
                std::find(and_or_nodes.cbegin(), and_or_nodes.cend(), inv_fanout[0]) == and_or_nodes.cend())
            {
                const auto fc            = fanins(fo_ntk, inv_fanout[0]);
                const auto fanin_inv     = fc.fanin_nodes;
                const auto do_substitute = std::all_of(fanin_inv.cbegin(), fanin_inv.cend(),
                                                       [this](const auto& fi_node) { return fo_ntk.is_inv(fi_node); });

                if (do_substitute && fanin_inv.size() > 1)
                {
                    x_inv_ao.emplace_back(fanin_inv[1]);
                    and_or_nodes.emplace_back(inv_fanout[0]);
                    m_inv_ao.emplace_back(fanin_inv[0]);
                }
            }
        }

        if (mode == operation_mode::AND_OR_ONLY || mode == operation_mode::ALL_NODES)
        {
            /*
             * If n = m_inv_ao: use children of and_or_nodes
             * If n = and_or_nodes: use m_inv_ao as child
             * If n = x_inv_ao: no children, because this node will not be in the network
             * */

            mockturtle::node<Ntk> fn{};
            // If n = m_inv_ao: use children of and_or_nodes
            for (std::size_t i = 0; i < m_inv_ao.size(); ++i)
            {
                if (m_inv_ao[i] == n)
                {
                    const auto and_node = and_or_nodes[i];
                    ntk.foreach_fanin(and_node,
                                      [this, &and_node, &old2new, &fn, &children](const auto& f)
                                      {
                                          const auto fis = fanins(fo_ntk, f);
                                          assert(fis.fanin_nodes.size() == 1);
                                          fn                    = fis.fanin_nodes[0];
                                          const auto tgt_signal = old2new[fn];
                                          // in this case one child is not placed yet
                                          if (tgt_signal == 0)
                                          {
                                              std::array<mockturtle::node<Ntk>, 3> nodes = {
                                                  m_inv_ao.back(), and_or_nodes.back(), x_inv_ao.back()};
                                              delayed_nodes[fn] = nodes;
                                          }
                                          else
                                          {
                                              children.emplace_back(tgt_signal);
                                          }
                                      });
                    return children;
                }
            }
            // If n = and_or_nodes: use m_inv_ao as child
            for (std::size_t i = 0; i < and_or_nodes.size(); ++i)
            {
                if (and_or_nodes[i] == n)
                {
                    fn                    = m_inv_ao[i];
                    const auto tgt_signal = old2new[fn];
                    children.emplace_back(tgt_signal);
                    return children;
                }
            }
        }

        // compute children of affected nodes
        ntk.foreach_fanin(n,
                          [this, &old2new, &children, &n](const auto& f)
                          {
                              auto fn = ntk.get_node(f);

                              if (mode == operation_mode::FO_ONLY || mode == operation_mode::ALL_NODES)
                              {
                                  for (std::size_t i = 0; i < fo_nodes.size(); ++i)
                                  {
                                      if (x_inv_fo[i] == fn)
                                      {
                                          // const auto fis = fanins(fo_ntk, fn);
                                          fn = m_inv_fo[i];
                                          break;
                                      }
                                  }
                              }

                              const auto tgt_signal = old2new[fn];
                              children.emplace_back(tgt_signal);
                          });

        return children;
    }

    template <typename NtkSrc>
    void connect_children_to_gates_affected(Ntk&                                                   ntk_dest,
                                            mockturtle::node_map<mockturtle::signal<Ntk>, NtkSrc>& old2new,
                                            const mockturtle::node<Ntk>&                           g,
                                            const std::vector<typename Ntk::signal>&               children)
    {
        // map all affected nodes for fo
        if constexpr (fiction::has_is_inv_v<TopoNtkSrc> && mockturtle::has_create_buf_v<Ntk>)
        {
            if (ntk.is_inv(g) && std::find(m_inv_fo.cbegin(), m_inv_fo.cend(), g) != m_inv_fo.cend())
            {
                old2new[g] = ntk_dest.create_buf(children[0]);
                return;  // keep looping
            }
            if (const auto po_it = std::find(x_inv_fo.cbegin(), x_inv_fo.cend(), g);
                ntk.is_inv(g) && po_it != x_inv_fo.cend())
            {
                if (ntk.is_po(g))
                {
                    // Preserve Outputs
                    const auto index = po_it - x_inv_fo.cbegin();
                    preserved_po.push_back(m_inv_fo[static_cast<std::vector<uint64_t>::size_type>(index)]);
                }
                return;  // keep looping
            }
        }
        if constexpr (fiction::has_is_buf_v<TopoNtkSrc> && mockturtle::has_create_not_v<Ntk>)
        {
            if (ntk.is_buf(g) && std::find(fo_nodes.cbegin(), fo_nodes.cend(), g) != fo_nodes.cend())
            {
                old2new[g] = ntk_dest.create_not(children[0]);
                return;  // keep looping
            }
        }
        // map all affected nodes for and/or
        if constexpr (fiction::has_is_inv_v<TopoNtkSrc> && mockturtle::has_is_and_v<TopoNtkSrc> &&
                      mockturtle::has_is_or_v<TopoNtkSrc> && mockturtle::has_create_and_v<Ntk> &&
                      mockturtle::has_create_or_v<Ntk>)
        {
            if (const auto po_it = std::find(m_inv_ao.cbegin(), m_inv_ao.cend(), g);
                ntk.is_inv(g) && po_it != m_inv_ao.cend())
            {
                const auto index = po_it - m_inv_ao.cbegin();
                // differentiate between and/or nodes
                if (ntk.is_and(and_or_nodes[static_cast<std::vector<uint64_t>::size_type>(index)]))
                {
                    old2new[g] = ntk_dest.create_or(children[0], children[1]);
                }
                else if (ntk.is_or(and_or_nodes[static_cast<std::vector<uint64_t>::size_type>(index)]))
                {
                    old2new[g] = ntk_dest.create_and(children[0], children[1]);
                }
                return;  // keep looping
            }
            if (ntk.is_inv(g) && std::find(x_inv_ao.cbegin(), x_inv_ao.cend(), g) != x_inv_ao.cend())
            {
                // inverter can be safely deleted, since it has the and/ornode as fanout
                return;  // keep looping
            }
        }
        if constexpr (mockturtle::has_is_and_v<TopoNtkSrc> && mockturtle::has_is_or_v<TopoNtkSrc> &&
                      mockturtle::has_create_not_v<Ntk>)
        {
            if ((ntk.is_and(g) || ntk.is_or(g)) &&
                std::find(and_or_nodes.cbegin(), and_or_nodes.cend(), g) != and_or_nodes.cend())
            {
                old2new[g] = ntk_dest.create_not(children[0]);
                return;  // keep looping
            }
        }
        // map all unaffected nodes
        if (connect_children_to_gates_unaffected(ntk, ntk_dest, old2new, g, children))
        {
            return;  // keep looping
        }
    }
};

}  // namespace detail

/**
 * A network optimization algorithm that substitutes inverters at the outputs of all fan-out nodes with one single
 * inverter at their inputs.
 * Thereby, the total number of inverters is reduced.
 * This is part of the Signal Distribution Networks I: Input Ordering.
 *
 * @tparam Ntk Logic network type.
 * @param ntk The input logic network whose inverter count is to be optimized.
 * @return A network that is logically equivalent to `ntk`, but with an optimized inverter count.
 */
template <typename Ntk>
Ntk inverter_substitution(const Ntk& ntk, detail::operation_mode mode = detail::operation_mode::FO_ONLY)
{
    static_assert(mockturtle::is_network_type_v<Ntk>, "Ntk is not a network type");
    static_assert(mockturtle::has_get_node_v<Ntk>, "Ntk does not implement the get_node function");
    static_assert(mockturtle::has_is_complemented_v<Ntk>, "Ntk does not implement the is_complemented function");
    static_assert(mockturtle::has_foreach_pi_v<Ntk>, "Ntk does not implement the foreach_pi function");
    static_assert(mockturtle::has_foreach_gate_v<Ntk>, "Ntk does not implement the foreach_gate function");
    static_assert(mockturtle::has_foreach_po_v<Ntk>, "Ntk does not implement the foreach_po function");
    static_assert(mockturtle::has_foreach_fanin_v<Ntk>, "Ntk does not implement the foreach_fanin function");
    static_assert(mockturtle::has_get_constant_v<Ntk>, "Ntk does not implement the get_constant function");
    static_assert(mockturtle::has_create_pi_v<Ntk>, "Ntk does not implement the create_pi function");
    static_assert(mockturtle::has_create_po_v<Ntk>, "Ntk does not implement the create_po function");
    static_assert(fiction::has_is_inv_v<Ntk>, "Ntk does not implement the is_inv function");

    static_assert(has_is_fanout_v<Ntk>, "Ntk does not implement the has_is_fanout function");

    assert(ntk.is_combinational() && "Network has to be combinational");

    /*auto                                    result = ntk;
    detail::inverter_substitution_impl<Ntk> p{result, mode};
    result = p.run();*/

    bool run    = true;
    auto result = ntk;
    while (run)
    {
        detail::inverter_substitution_impl<Ntk> p{result, mode};
        if (!p.is_rerun())
        {
            break;  // premature termination
        }
        result = p.run();
        run    = p.is_rerun();
    }

    return result;
}

}  // namespace fiction

#endif  // FICTION_INVERTER_SUBSTITUTION_HPP
