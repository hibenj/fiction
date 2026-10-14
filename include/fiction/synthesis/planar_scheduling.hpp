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
 * @brief Moves the gates and fanout nodes of a planar, balanced network between levels to remove buffers, keeping
 * the planar embedding.
 * @author Benjamin Hien (hibenj)
 */

#pragma once

#include "fiction/networks/name_utils.hpp"
#include "fiction/networks/network_utils.hpp"
#include "fiction/utils/progress.hpp"

#include <mockturtle/traits.hpp>
#include <mockturtle/utils/node_map.hpp>
#include <mockturtle/utils/stopwatch.hpp>
#include <mockturtle/views/fanout_view.hpp>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <utility>
#include <vector>

namespace fiction::synthesis
{

/**
 * Parameters of the planar scheduling.
 */
struct planar_scheduling_params
{
    /**
     * Upper bound on the number of passes, each a sweep of gate moves downwards followed by a sweep of fanout moves
     * upwards. The passes stop earlier as soon as one removes no node.
     */
    uint32_t max_passes = 100;
    /**
     * Reports completed passes.
     */
    utils::progress_callback on_progress{};
};

/**
 * Statistics of the planar scheduling.
 */
struct planar_scheduling_stats
{
    /**
     * Nodes of the input, inputs not counted.
     */
    uint64_t nodes_before{0};
    /**
     * Nodes of the result, inputs not counted.
     */
    uint64_t nodes_after{0};
    /**
     * Chain buffers (buffers with one consumer) of the input.
     */
    uint64_t buffers_before{0};
    /**
     * Chain buffers of the result.
     */
    uint64_t buffers_after{0};
    /**
     * Gates moved one level down.
     */
    uint64_t gates_moved_down{0};
    /**
     * Inverters moved one level down.
     */
    uint64_t inverters_moved_down{0};
    /**
     * Fanout nodes moved one level up.
     */
    uint64_t fanouts_moved_up{0};
    /**
     * Passes run.
     */
    uint32_t passes{0};
    /**
     * Total runtime.
     */
    mockturtle::stopwatch<>::duration time_total{0};
    /**
     * Prints the statistics.
     *
     * @param out Output stream.
     */
    void report(std::ostream& out = std::cout) const
    {
        out << "[i] nodes = " << nodes_before << " -> " << nodes_after << " (chain buffers " << buffers_before << " -> "
            << buffers_after << ")\n";
        out << "[i] moves = " << gates_moved_down << " gates down, " << inverters_moved_down << " inverters down, "
            << fanouts_moved_up << " fanouts up in " << passes << " passes\n";
        out << "[i] total time = " << mockturtle::to_seconds(time_total) << " s\n";
    }
};

namespace detail
{

/**
 * Implementation of the planar scheduling.
 *
 * @tparam Ntk Ranked network type.
 */
template <typename Ntk>
class planar_scheduling_impl
{
  public:
    /**
     * Node type.
     */
    using node = mockturtle::node<Ntk>;
    /**
     * Signal type.
     */
    using signal = mockturtle::signal<Ntk>;

    /**
     * Creates the implementation.
     *
     * @param src Input network.
     * @param p Parameters.
     * @param st Statistics to fill.
     */
    planar_scheduling_impl(const Ntk& src, planar_scheduling_params p, planar_scheduling_stats& st) :
            ntk{src},
            ps{std::move(p)},
            pst{st}
    {}

    /**
     * Runs the scheduling.
     *
     * @return The scheduled network.
     */
    [[nodiscard]] Ntk run()
    {
        const mockturtle::stopwatch stop{pst.time_total};

        read();
        pst.nodes_before   = count_nodes();
        pst.buffers_before = count_buffers();

        utils::progress_reporter progress{ps.on_progress, "scheduling", ps.max_passes};
        for (uint32_t pass = 0; pass < ps.max_passes; ++pass)
        {
            ++pst.passes;
            const auto before = count_nodes();
            sweep_down();
            sweep_up();
            progress.advance();
            if (count_nodes() >= before)
            {
                break;
            }
        }

        pst.nodes_after   = count_nodes();
        pst.buffers_after = count_buffers();

        return write();
    }

  private:
    /**
     * What a node of the layered structure is.
     */
    enum class kind : uint8_t
    {
        INPUT,
        GATE,
        INVERTER,
        BUFFER,
        FANOUT
    };
    /**
     * A node of the layered structure.
     */
    struct lnode
    {
        kind                 k{kind::BUFFER};
        node                 source{};   // the source node (for cloning gates and for inputs)
        std::vector<int32_t> fanins{};   // in the source's fanin order
        std::vector<int32_t> fanouts{};  // consumers, any order
        uint32_t             level{0};
        bool                 po{false};   // drives at least one primary output
        bool                 live{true};  // false once a move removed the node
    };

    /**
     * Reads the source network into the layered structure, in rank order.
     */
    void read()
    {
        const mockturtle::fanout_view      fanouts{ntk};
        mockturtle::node_map<int32_t, Ntk> id{ntk, -1};

        order.assign(ntk.depth() + 1, {});
        for (uint32_t level = 0; level <= ntk.depth(); ++level)
        {
            ntk.foreach_node_in_rank(level,
                                     [this, level, &fanouts, &id](const auto& n)
                                     {
                                         if (ntk.is_constant(n))
                                         {
                                             return;
                                         }
                                         lnode ln{};
                                         ln.source = n;
                                         ln.level  = level;
                                         if (ntk.is_pi(n))
                                         {
                                             ln.k = kind::INPUT;
                                         }
                                         else if (ntk.is_buf(n))
                                         {
                                             ln.k = fanouts.fanout_size(n) > 1 ? kind::FANOUT : kind::BUFFER;
                                         }
                                         else if (ntk.is_inv(n))
                                         {
                                             ln.k = kind::INVERTER;
                                         }
                                         else
                                         {
                                             ln.k = kind::GATE;
                                         }
                                         ntk.foreach_fanin(n,
                                                           [this, &ln, &id](const auto& f)
                                                           {
                                                               const auto fn = ntk.get_node(f);
                                                               if (ntk.is_constant(fn))
                                                               {
                                                                   throw std::invalid_argument(
                                                                       "constant fanins are not supported");
                                                               }
                                                               assert(id[fn] >= 0);
                                                               ln.fanins.push_back(id[fn]);
                                                           });
                                         const auto me = static_cast<int32_t>(nodes.size());
                                         id[n]         = me;
                                         for (const auto f : ln.fanins)
                                         {
                                             nodes[static_cast<std::size_t>(f)].fanouts.push_back(me);
                                         }
                                         nodes.push_back(std::move(ln));
                                         order[level].push_back(me);
                                     });
        }

        // a constant output is kept as one: `CONSTANT_FALSE` or `CONSTANT_TRUE` instead of a node
        ntk.foreach_po(
            [this, &id](const auto& po)
            {
                const auto n = ntk.get_node(po);
                if (ntk.is_constant(n))
                {
                    po_nodes.push_back(ntk.constant_value(n) ? CONSTANT_TRUE : CONSTANT_FALSE);
                    return;
                }
                po_nodes.push_back(id[n]);
                nodes[static_cast<std::size_t>(id[n])].po = true;
            });
    }

    [[nodiscard]] uint64_t count_nodes() const noexcept
    {
        return static_cast<uint64_t>(
            std::count_if(nodes.begin(), nodes.end(), [](const lnode& n) { return n.k != kind::INPUT && n.live; }));
    }

    [[nodiscard]] uint64_t count_buffers() const noexcept
    {
        return static_cast<uint64_t>(
            std::count_if(nodes.begin(), nodes.end(), [](const lnode& n) { return n.k == kind::BUFFER && n.live; }));
    }

    /**
     * The position of a node in its level.
     */
    [[nodiscard]] std::size_t position(const int32_t n) const
    {
        const auto& level = order[nodes[static_cast<std::size_t>(n)].level];
        const auto  it    = std::find(level.begin(), level.end(), n);
        assert(it != level.end());
        return static_cast<std::size_t>(std::distance(level.begin(), it));
    }

    /**
     * Replaces `from` by `to` in the fanins of `consumer`.
     */
    void replace_fanin(const int32_t consumer, const int32_t from, const int32_t to)
    {
        for (auto& f : nodes[static_cast<std::size_t>(consumer)].fanins)
        {
            if (f == from)
            {
                f = to;
            }
        }
    }

    /**
     * Replaces `from` by `to` in the fanouts of `source`.
     */
    void replace_fanout(const int32_t source, const int32_t from, const int32_t to)
    {
        for (auto& f : nodes[static_cast<std::size_t>(source)].fanouts)
        {
            if (f == from)
            {
                f = to;
            }
        }
    }

    /**
     * Moves gates and inverters one level down wherever every fanin arrives through a chain buffer: the gate takes
     * the place of its fanin buffers on the level below (they are consecutive in a planar drawing), and one buffer
     * takes the gate's old place to carry its output. A gate with k fanins trades k buffers for one.
     */
    void sweep_down()
    {
        for (uint32_t level = 2; level < order.size(); ++level)
        {
            // the level changes while moving, so work on a snapshot
            const auto snapshot = order[level];
            for (const auto g : snapshot)
            {
                auto& gn = nodes[static_cast<std::size_t>(g)];
                if (gn.k != kind::GATE && gn.k != kind::INVERTER)
                {
                    continue;
                }
                if (!std::all_of(gn.fanins.begin(), gn.fanins.end(),
                                 [this, level](const int32_t f)
                                 {
                                     const auto& fn = nodes[static_cast<std::size_t>(f)];
                                     return fn.k == kind::BUFFER && fn.level == level - 1 && fn.fanouts.size() == 1;
                                 }))
                {
                    continue;
                }
                // the fanin buffers must be consecutive on the level below
                std::vector<std::size_t> positions{};
                for (const auto f : gn.fanins)
                {
                    positions.push_back(position(f));
                }
                std::sort(positions.begin(), positions.end());
                if (positions.back() - positions.front() + 1 != positions.size())
                {
                    continue;
                }

                // the gate reads what its buffers read
                for (auto& f : gn.fanins)
                {
                    const auto buffer = f;
                    const auto src    = nodes[static_cast<std::size_t>(buffer)].fanins.front();
                    replace_fanout(src, buffer, g);
                    f                                            = src;
                    nodes[static_cast<std::size_t>(buffer)].live = false;
                }
                auto& below = order[level - 1];
                below.erase(below.begin() + static_cast<std::ptrdiff_t>(positions.front()),
                            below.begin() + static_cast<std::ptrdiff_t>(positions.back() + 1));
                below.insert(below.begin() + static_cast<std::ptrdiff_t>(positions.front()), g);

                // a buffer carries the output from the gate's old place
                const auto nb = static_cast<int32_t>(nodes.size());
                lnode      bn{};
                bn.k       = kind::BUFFER;
                bn.level   = level;
                bn.fanins  = {g};
                bn.fanouts = gn.fanouts;
                bn.po      = gn.po;
                for (const auto c : gn.fanouts)
                {
                    replace_fanin(c, g, nb);
                }
                for (auto& p : po_nodes)
                {
                    if (p == g)
                    {
                        p = nb;
                    }
                }
                gn.fanouts = {nb};
                gn.po      = false;
                gn.level   = level - 1;
                nodes.push_back(std::move(bn));
                auto& here                              = order[level];
                *std::find(here.begin(), here.end(), g) = nb;

                if (nodes[static_cast<std::size_t>(g)].k == kind::GATE)
                {
                    ++pst.gates_moved_down;
                }
                else
                {
                    ++pst.inverters_moved_down;
                }
            }
        }
    }

    /**
     * Moves fanout nodes one level up wherever every consumer is reached through a chain buffer: the fanout node
     * takes the place of its output buffers on the level above (consecutive in a planar drawing), and one buffer
     * takes its old place to carry its input. A fanout node with m consumers trades m buffers for one.
     */
    void sweep_up()
    {
        // from the level below the top down to level 1
        for (auto level = static_cast<uint32_t>(order.size()) - 1u; level > 0u; --level)
        {
            if (level + 1 >= order.size())
            {
                continue;
            }
            const auto snapshot = order[level];
            for (const auto f : snapshot)
            {
                auto& fn = nodes[static_cast<std::size_t>(f)];
                if (fn.k != kind::FANOUT || fn.po)
                {
                    continue;
                }
                if (!std::all_of(fn.fanouts.begin(), fn.fanouts.end(),
                                 [this, level](const int32_t c)
                                 {
                                     const auto& cn = nodes[static_cast<std::size_t>(c)];
                                     return cn.k == kind::BUFFER && cn.level == level + 1 && cn.fanouts.size() == 1;
                                 }))
                {
                    continue;
                }
                std::vector<std::size_t> positions{};
                for (const auto c : fn.fanouts)
                {
                    positions.push_back(position(c));
                }
                std::sort(positions.begin(), positions.end());
                if (positions.back() - positions.front() + 1 != positions.size())
                {
                    continue;
                }

                // the fanout node drives what its buffers drove, in the buffers' left-to-right order
                std::vector<int32_t> buffers = fn.fanouts;
                std::sort(buffers.begin(), buffers.end(),
                          [this](const int32_t a, const int32_t b) { return position(a) < position(b); });
                std::vector<int32_t> consumers{};
                bool                 po = false;
                for (const auto buffer : buffers)
                {
                    auto& bn = nodes[static_cast<std::size_t>(buffer)];
                    for (const auto c : bn.fanouts)
                    {
                        replace_fanin(c, buffer, f);
                        consumers.push_back(c);
                    }
                    for (auto& p : po_nodes)
                    {
                        if (p == buffer)
                        {
                            p  = f;
                            po = true;
                        }
                    }
                    bn.live = false;
                }
                auto& above = order[level + 1];
                above.erase(above.begin() + static_cast<std::ptrdiff_t>(positions.front()),
                            above.begin() + static_cast<std::ptrdiff_t>(positions.back() + 1));
                above.insert(above.begin() + static_cast<std::ptrdiff_t>(positions.front()), f);

                // a buffer carries the input to the fanout node's old place
                const auto nb  = static_cast<int32_t>(nodes.size());
                const auto src = fn.fanins.front();
                lnode      bn{};
                bn.k       = kind::BUFFER;
                bn.level   = level;
                bn.fanins  = {src};
                bn.fanouts = {f};
                replace_fanout(src, f, nb);
                fn.fanins  = {nb};
                fn.fanouts = std::move(consumers);
                fn.po      = po;
                fn.level   = level + 1;
                nodes.push_back(std::move(bn));
                auto& here                              = order[level];
                *std::find(here.begin(), here.end(), f) = nb;

                ++pst.fanouts_moved_up;
            }
        }
    }

    /**
     * Writes the layered structure as a network of the source type, level by level in left-to-right order, so
     * that the ranks of the result are the embedding.
     */
    [[nodiscard]] Ntk write()
    {
        auto  init    = networks::initialize_copy_network_with_virtual_pis(ntk);
        auto& dest    = init.first;
        auto& old2new = init.second;

        std::vector<signal> sig(nodes.size());
        std::vector<node>   rank0{};
        for (const auto n : order[0])
        {
            sig[static_cast<std::size_t>(n)] = old2new[nodes[static_cast<std::size_t>(n)].source];
            rank0.push_back(dest.get_node(sig[static_cast<std::size_t>(n)]));
        }

        for (uint32_t level = 1; level < order.size(); ++level)
        {
            for (const auto n : order[level])
            {
                const auto&         ln = nodes[static_cast<std::size_t>(n)];
                std::vector<signal> fanins{};
                for (const auto f : ln.fanins)
                {
                    fanins.push_back(sig[static_cast<std::size_t>(f)]);
                }
                switch (ln.k)
                {
                    case kind::BUFFER:
                    case kind::FANOUT:
                    {
                        sig[static_cast<std::size_t>(n)] = dest.create_buf(fanins.front());
                        break;
                    }
                    case kind::INVERTER:
                    {
                        sig[static_cast<std::size_t>(n)] = dest.create_not(fanins.front());
                        break;
                    }
                    case kind::GATE:
                    {
                        sig[static_cast<std::size_t>(n)] = dest.clone_node(ntk, ln.source, fanins);
                        break;
                    }
                    case kind::INPUT:
                    {
                        break;
                    }
                }
            }
        }

        for (const auto p : po_nodes)
        {
            if (p == CONSTANT_FALSE || p == CONSTANT_TRUE)
            {
                dest.create_po(dest.get_constant(p == CONSTANT_TRUE));
                continue;
            }
            dest.create_po(sig[static_cast<std::size_t>(p)]);
        }

        networks::restore_names(ntk, dest, old2new);

        dest.update_ranks();
        dest.set_ranks(0, rank0);

        return dest;
    }

    const Ntk&                     ntk;
    const planar_scheduling_params ps;
    planar_scheduling_stats&       pst;

    static constexpr int32_t CONSTANT_FALSE = -1;
    static constexpr int32_t CONSTANT_TRUE  = -2;

    std::vector<lnode>                nodes{};
    std::vector<std::vector<int32_t>> order{};
    std::vector<int32_t>              po_nodes{};  // node ids, or a constant marker
};

}  // namespace detail

/**
 * Removes buffers from a planar, balanced, fanout-substituted network by moving its gates and fanout nodes between
 * levels while keeping the planar embedding. A gate whose every fanin arrives through a chain buffer moves one level
 * down and takes the place of those buffers; one buffer takes its old place to carry the output, so k buffers
 * become one. A fanout node whose every consumer is reached through a chain buffer moves one level up and takes
 * the place of those buffers; one buffer takes its old place, so m buffers become one. Inverters move down with
 * the gates, which costs nothing and lets the gate above them move. Both moves keep the left-to-right order of
 * every level, because the fanin buffers of a gate and the output buffers of a fanout node are consecutive in a
 * planar drawing, so the result is planar when the input is. The passes stop as soon as one removes no node.
 *
 * The rank order of every level of the result is the embedding; the inputs keep the input's rank order and its
 * own order; the outputs stay on the top level.
 *
 * @tparam Ntk Ranked network type (see `mutable_rank_view`) that supports `create_buf`, `create_not`, `clone_node`,
 * `is_buf` and `is_inv`; planar, balanced, fanout-substituted, without constant fanins (constant outputs are kept).
 * @param ntk The ranked, planar input network.
 * @param ps Parameters.
 * @param pst Statistics.
 * @return The scheduled network of the same type, planar and balanced, with at most as many nodes.
 */
template <typename Ntk>
[[nodiscard]] Ntk planar_scheduling(const Ntk& ntk, const planar_scheduling_params& ps = {},
                                    planar_scheduling_stats* pst = nullptr)
{
    static_assert(mockturtle::is_network_type_v<Ntk>, "Ntk is not a network type");
    static_assert(mockturtle::has_rank_position_v<Ntk>, "Ntk does not implement the rank_position function");
    static_assert(mockturtle::has_foreach_node_in_rank_v<Ntk>, "Ntk does not implement foreach_node_in_rank");
    static_assert(mockturtle::has_depth_v<Ntk>, "Ntk does not implement the depth function");
    static_assert(mockturtle::has_is_buf_v<Ntk>, "Ntk does not implement the is_buf function");
    static_assert(mockturtle::has_create_buf_v<Ntk>, "Ntk does not implement the create_buf function");
    static_assert(mockturtle::has_create_not_v<Ntk>, "Ntk does not implement the create_not function");
    static_assert(mockturtle::has_clone_node_v<Ntk>, "Ntk does not implement the clone_node function");

    planar_scheduling_stats             st{};
    detail::planar_scheduling_impl<Ntk> impl{ntk, ps, st};
    auto                                result = impl.run();
    if (pst != nullptr)
    {
        *pst = st;
    }
    return result;
}

}  // namespace fiction::synthesis
