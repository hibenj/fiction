/*
 * Copyright (c) 2018 - 2023 Marcel Walter
 * Copyright (c) 2023 - present Chair for Design Automation, Technical University of Munich
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

/**
 * @file
 * @brief Path-balances a logic network with identity-computing buffer nodes.
 * @author Marcel Walter (marcelwa)
 * @author Benjamin Hien (hibenj)
 */

#pragma once

#include "fiction/synthesis/network_conversion.hpp"
#include "fiction/utils/progress.hpp"

#include <mockturtle/traits.hpp>
#include <mockturtle/utils/node_map.hpp>
#include <mockturtle/views/depth_view.hpp>
#include <mockturtle/views/topo_view.hpp>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

namespace fiction::synthesis
{

/**
 * Parameters for the network balancing algorithm.
 */
struct network_balancing_params
{
    /**
     * Flag to indicate that all output nodes should be in the same rank.
     */
    bool unify_outputs = false;
    /**
     * Whether primary outputs driven by a constant receive a buffer chain when unifying outputs. Placement flows that
     * do not place constants set this to `false`: such outputs then stay unbuffered, and `is_balanced` ignores them.
     */
    bool buffer_constant_outputs = true;
    /**
     * Whether the buffers of one source are shared by all its fanouts. By default, every fanin edge that spans levels
     * receives its own buffer chain, so a node with `k` consumers above it carries `k` parallel chains and the fanout
     * stays at the node. With this flag, one buffer per source and level is created and read by every consumer on
     * the level above it, so the chains form a tree whose branches sit right below the consumers. This never needs
     * more buffers, and it removes the parallel chain edges that make up most of the crossings of wide networks.
     * Complemented primary outputs keep their own chain when outputs are unified.
     */
    bool share_buffers = false;
    /** @brief Reports completed work in each bounded phase. */
    utils::progress_callback on_progress{};
};

namespace detail
{

template <typename Ntk>
std::vector<uint32_t> get_po_levels(const Ntk& ntk)
{
    std::vector<uint32_t> po_levels{};
    ntk.foreach_po([&ntk, &po_levels](const auto& po) { po_levels.emplace_back(ntk.level(ntk.get_node(po))); });

    return po_levels;
}

template <typename NtkDest, typename NtkSrc>
class network_balancing_impl
{
  public:
    /**
     * @brief Stores the network and algorithm parameters.
     * @param src Source network.
     * @param p Algorithm parameters.
     */
    network_balancing_impl(const NtkSrc& src, network_balancing_params p) :
            ntk{convert_network<NtkDest>(src)},
            ntk_topo{ntk},
            ntk_depth{ntk},
            ps{std::move(p)}
    {}

    NtkDest run()
    {
        // insert num buffers after child in balance_ntk
        const auto insert_buf_chain = [](auto& balance_ntk, const auto& num, auto& child)
        {
            for (auto i = 0u; i < num; ++i)
            {
                child = balance_ntk.create_buf(child);
            }
        };

        // initialize a network copy
        auto init = mockturtle::initialize_copy_network<NtkDest>(ntk_topo);

        auto& balanced = init.first;
        auto& old2new  = init.second;

        // the shared chain of every source: entry `i` is the buffer `i + 1` levels above the source
        mockturtle::node_map<std::vector<typename NtkDest::signal>, mockturtle::topo_view<NtkDest>> shared_chains{
            ntk_topo};

        // the signal of source `fn` lifted by `num` levels: a private chain, or the shared chain extended as needed
        const auto lifted =
            [this, &balanced, &old2new, &insert_buf_chain, &shared_chains](const auto& fn, const auto& num)
        {
            auto child = old2new[fn];

            if (!ps.share_buffers)
            {
                insert_buf_chain(balanced, num, child);
                return child;
            }

            auto& chain = shared_chains[fn];
            if (!chain.empty())
            {
                child = chain.back();
            }
            while (chain.size() < num)
            {
                child = balanced.create_buf(child);
                chain.push_back(child);
            }

            return num == 0 ? old2new[fn] : chain[num - 1];
        };

        utils::progress_reporter progress{ps.on_progress, "balancing gates", ntk_topo.num_gates()};
        ntk_topo.foreach_gate(
            [this, &balanced, &old2new, &lifted, &progress](const auto& n)
            {
                // gather children, but substitute fanins by buf where applicable
                std::vector<typename mockturtle::topo_view<NtkDest>::signal> children{};

                ntk_topo.foreach_fanin(n,
                                       [this, &n, &old2new, &children, &lifted](const auto& f)
                                       {
                                           const auto fn = ntk_topo.get_node(f);

                                           // do not balance constants
                                           if (ntk_topo.is_constant(fn))
                                           {
                                               children.push_back(old2new[fn]);
                                               return;
                                           }

                                           children.push_back(lifted(fn, ntk_depth.level(n) - ntk_depth.level(fn) - 1));
                                       });

                // clone the node with new children according to its depth
                old2new[n] = balanced.clone_node(ntk_topo, n, children);
                progress.advance();
            });

        // gather PO levels
        const auto po_levels    = get_po_levels(ntk_depth);
        const auto max_po_level = po_levels.empty() ? 0u : *std::ranges::max_element(po_levels);

        // add primary outputs to finalize the network
        ntk_topo.foreach_po(
            [this, &old2new, &balanced, &po_levels, &max_po_level, &insert_buf_chain, &lifted](const auto& po, auto i)
            {
                const auto po_node    = ntk_topo.get_node(po);
                const auto tgt_signal = old2new[po_node];

                auto tgt_po = ntk_topo.is_complemented(po) ? balanced.create_not(tgt_signal) : tgt_signal;

                if (ps.unify_outputs && (ps.buffer_constant_outputs || !ntk_topo.is_constant(po_node)))
                {
                    const auto num = max_po_level - po_levels[i];

                    // an uncomplemented output of a non-constant node can share the node's chain
                    if (ntk_topo.is_complemented(po) || ntk_topo.is_constant(po_node))
                    {
                        insert_buf_chain(balanced, num, tgt_po);
                    }
                    else
                    {
                        tgt_po = lifted(po_node, num);
                    }
                }

                balanced.create_po(tgt_po);
            });

        // restore signal names if applicable
        networks::restore_names(ntk_topo, balanced, old2new);

        return balanced;
    }

  private:
    NtkDest ntk;

    mockturtle::topo_view<NtkDest>  ntk_topo;
    mockturtle::depth_view<NtkDest> ntk_depth;

    const network_balancing_params ps;
};

template <typename Ntk>
class is_balanced_impl
{
  public:
    /**
     * @brief Stores the network and algorithm parameters.
     * @param src Source network.
     * @param p Algorithm parameters.
     */
    is_balanced_impl(const Ntk& src, network_balancing_params p) : ntk{src}, ntk_depth{src}, ps{std::move(p)} {}

    bool run()
    {
        ntk.foreach_gate(
            [this](const auto& n)
            {
                ntk.foreach_fanin(n,
                                  [this, &n](const auto& f)
                                  {
                                      const auto fn = ntk.get_node(f);

                                      // skip constants
                                      if (!ntk.is_constant(fn))
                                      {
                                          if (const auto level_diff = ntk_depth.level(n) - ntk_depth.level(fn) - 1;
                                              level_diff > 0)
                                          {
                                              balanced = false;
                                          }
                                      }

                                      return balanced;
                                  });

                return balanced;
            });

        if (ps.unify_outputs)
        {
            // unbuffered constant outputs do not take part in the comparison
            std::vector<uint32_t> po_levels{};
            po_levels.reserve(ntk.num_pos());
            ntk.foreach_po(
                [this, &po_levels](const auto& po)
                {
                    if (const auto n = ntk.get_node(po); ps.buffer_constant_outputs || !ntk.is_constant(n))
                    {
                        po_levels.push_back(ntk_depth.level(n));
                    }
                });

            // check whether POs with different depth levels exist
            if (std::ranges::adjacent_find(po_levels, std::not_equal_to<>()) != po_levels.end())
            {
                balanced = false;
            }
        }

        return balanced;
    }

  private:
    const Ntk& ntk;

    const mockturtle::depth_view<Ntk> ntk_depth;

    const network_balancing_params ps;

    bool balanced = true;
};

}  // namespace detail

/**
 * Balances a logic network with buffer nodes that compute the identity function. For this purpose, `create_buf` is
 * utilized. Therefore, `NtkDest` should support identity nodes. If it does not, no new nodes will in fact be created.
 * In either case, the returned network will be logically equivalent to the input one.
 *
 * The process is not combined with fanout substitution. By default, every edge that spans levels receives its own
 * buffer chain; with `network_balancing_params::share_buffers`, the chains of one source are shared level by level,
 * which needs no more buffers and leaves one chain edge per source and level instead of one per consumer.
 *
 * The returned network is newly created from scratch because its type `NtkDest` may differ from `NtkSrc`.
 *
 * @note The physical design algorithms natively provided in fiction do not require their input networks to be balanced.
 * If that is necessary, they will do it themselves. Providing already balanced networks may lead to substantial
 * overhead.
 *
 * @tparam NtkDest Type of the returned logic network.
 * @tparam NtkSrc Type of the input logic network.
 * @param ntk_src The input logic network.
 * @param ps Parameters.
 * @return A path-balanced logic network of type `NtkDest` that is logically equivalent to `ntk_src`.
 */
template <typename NtkDest, typename NtkSrc>
NtkDest network_balancing(const NtkSrc& ntk_src, network_balancing_params ps = {})
{
    static_assert(mockturtle::is_network_type_v<NtkSrc>, "NtkSrc is not a network type");
    static_assert(mockturtle::is_network_type_v<NtkDest>, "NtkDest is not a network type");

    static_assert(mockturtle::has_get_node_v<NtkSrc>, "NtkSrc does not implement the get_node function");
    static_assert(mockturtle::has_is_complemented_v<NtkSrc>, "NtkSrc does not implement the is_complemented function");
    static_assert(mockturtle::has_foreach_gate_v<NtkSrc>, "NtkSrc does not implement the foreach_gate function");
    static_assert(mockturtle::has_foreach_fanin_v<NtkSrc>, "NtkSrc does not implement the foreach_fanin function");
    static_assert(mockturtle::has_foreach_po_v<NtkSrc>, "NtkSrc does not implement the foreach_po function");

    static_assert(mockturtle::has_create_pi_v<NtkDest>, "NtkDest does not implement the create_pi function");
    static_assert(mockturtle::has_create_not_v<NtkDest>, "NtkDest does not implement the create_not function");
    static_assert(mockturtle::has_create_po_v<NtkDest>, "NtkDest does not implement the create_po function");
    static_assert(mockturtle::has_create_buf_v<NtkDest>, "NtkDest does not implement the create_buf function");
    static_assert(mockturtle::has_clone_node_v<NtkDest>, "NtkDest does not implement the clone_node function");

    detail::network_balancing_impl<NtkDest, NtkSrc> p{ntk_src, ps};

    auto result = p.run();

    return result;
}
/**
 * Checks if a logic network is properly path-balanced with regard to the provided parameters.
 *
 * @tparam Ntk Logic network type.
 * @param ntk The logic network to check.
 * @param ps Parameters.
 * @return `true` iff `ntk` is properly path-balanced with regard to `ps`.
 */
template <typename Ntk>
bool is_balanced(const Ntk& ntk, network_balancing_params ps = {}) noexcept
{
    static_assert(mockturtle::is_network_type_v<Ntk>, "NtkSrc is not a network type");
    static_assert(mockturtle::has_foreach_gate_v<Ntk>, "Ntk does not implement the foreach_gate function");
    static_assert(mockturtle::has_foreach_fanin_v<Ntk>, "Ntk does not implement the foreach_fanin function");
    static_assert(mockturtle::has_foreach_po_v<Ntk>, "Ntk does not implement the foreach_po function");
    static_assert(mockturtle::has_get_node_v<Ntk>, "Ntk does not implement the get_node function");

    detail::is_balanced_impl<Ntk> p{ntk, ps};

    auto result = p.run();

    return result;
}

}  // namespace fiction::synthesis
