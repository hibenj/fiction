//
// Created by benjamin on 21.01.25.
//

#ifndef FICTION_ORTHOGONAL_PLANAR_V2_HPP
#define FICTION_ORTHOGONAL_PLANAR_V2_HPP

#include "fiction/algorithms/properties/check_planarity.hpp"
#include "fiction/layouts/clocking_scheme.hpp"
#include "fiction/traits.hpp"
#include "fiction/utils/network_utils.hpp"
#include "fiction/utils/placement_utils.hpp"

#include <fiction/algorithms/physical_design/orthogonal.hpp>

#include <mockturtle/traits.hpp>
#include <mockturtle/utils/node_map.hpp>
#include <mockturtle/utils/stopwatch.hpp>
#include <mockturtle/views/fanout_view.hpp>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <numeric>
#include <tuple>
#include <unordered_map>
#include <vector>

#if (PROGRESS_BARS)
#include <mockturtle/utils/progress_bar.hpp>
#endif

namespace fiction
{

namespace detail
{

template <typename Ntk, typename Lyt>
std::tuple<std::vector<uint8_t>, std::vector<uint8_t>, uint64_t>
compute_pr_variables(const Ntk& ntk, const Lyt& lyt, mockturtle::node_map<mockturtle::signal<Lyt>, Ntk> node2pos,
                     uint32_t lvl)
{
    std::vector<uint8_t> orientation(ntk.rank_width(lvl));
    std::vector<uint8_t> gap(ntk.rank_width(lvl));
    uint64_t             max_gap = 0;

    bool chain_flag       = false;
    bool orientation_flag = false;

    ntk.foreach_node_in_rank(
        lvl,
        [&ntk, &node2pos, &orientation_flag, &orientation, &gap, &max_gap, &chain_flag](const auto& n, const auto& i)
        {
            auto fc = fanins(ntk, n);
            // when there is a node with two fan-ins, we need to add empty diagonals for each empty
            // tile between the fan-in nodes (max_gap)
            if (fc.fanin_nodes.size() == 2)
            {
                std::sort(fc.fanin_nodes.begin(), fc.fanin_nodes.end(),
                          [&ntk](int a, int b) { return ntk.rank_position(a) < ntk.rank_position(b); });
                // compute the max_gap for two fan-ins of  anode
                const auto &pre1 = fc.fanin_nodes[0], &pre2 = fc.fanin_nodes[1];

                auto pre1_t = static_cast<tile<Lyt>>(node2pos[pre1]), pre2_t = static_cast<tile<Lyt>>(node2pos[pre2]);

                max_gap = std::max(max_gap, static_cast<uint64_t>(pre2_t.y - pre1_t.y));

                // compute the gap between nodes with both orientation 0
                // if orientation = 0, and we get a buffer with fan-in node = fan-out we need a gap
                if (ntk.is_fanout(pre2))
                {
                    chain_flag = true;
                    if (i > 0)
                    {
                        orientation[i] = orientation[i - 1];
                    }
                }
                else
                {
                    chain_flag     = false;
                    orientation[i] = 0;
                }

                // if this is the first non-buffer node in the level, set the orientation
                if (!orientation_flag)
                {
                    std::fill(orientation.begin(), orientation.begin() + i, 1);
                    orientation_flag = true;
                }
            }
            else if (fc.fanin_nodes.size() == 1)
            {
                const auto& pre = fc.fanin_nodes[0];
                if (ntk.is_fanout(pre) && ntk.fanout_size(pre) == 2)
                {
                    if (chain_flag)
                    {
                        chain_flag     = false;
                        orientation[i] = 1;
                    }
                    else
                    {
                        chain_flag = true;

                        // make sure the index is in range
                        if (i > 0)
                        {
                            // check if the last orientation is 1 and hence conflicting
                            if (orientation[i - 1] == 1)
                            {
                                gap[i] = 1;
                            }
                        }
                    }

                    // if this is the first non-buffer node in the level, set the orientation
                    if (!orientation_flag)
                    {
                        orientation_flag = true;
                    }
                }
                else if (i > 0)
                {
                    // this node is a buffer
                    assert(ntk.fanout_size(pre) == 1);

                    // buffer take the orientation of the preceding node in the level
                    orientation[i] = orientation[i - 1];
                }
            }
        });
    max_gap        = std::max(max_gap, static_cast<uint64_t>(std::accumulate(gap.cbegin(), gap.cend(), 0)));
    const auto ret = std::make_tuple(orientation, gap, max_gap);
    return ret;
}

template <typename Lyt, typename Ntk>
class orthogonal_planar_v2_impl
{
  public:
    orthogonal_planar_v2_impl(const Ntk& src, const orthogonal_physical_design_params& p,
                              orthogonal_physical_design_stats& st) :
            ntk{mockturtle::names_view(mockturtle::fanout_view(src))},
            ps{p},
            pst{st}
    {}

    Lyt run()
    {
        // measure run time
        mockturtle::stopwatch stop{pst.time_total};
        //
        using node = typename Ntk::node;

        mockturtle::node_map<mockturtle::signal<Lyt>, decltype(ntk)> node2pos{ntk};

        aspect_ratio<Lyt> size_ = {0, 0};

        // instantiate the layout
        Lyt layout{size_, twoddwave_clocking<Lyt>(ps.number_of_clock_phases)};

        // reserve PI nodes without positions
        auto pi2node = reserve_input_nodes(layout, ntk);

        // first x-pos to use for gates is 1 because PIs take up the 0th column
        tile<Lyt> latest_pos{1, 0};

#if (PROGRESS_BARS)
        // initialize a progress bar
        mockturtle::progress_bar bar{ctn.color_ntk.size(), "[i] arranging layout: |{0}|"};
#endif

        tile<Lyt> prec_pos{0, 0};
        tile<Lyt> first_pos = {ntk.num_pis(), 0};

        for (uint32_t lvl = 0; lvl < ntk.depth() + 1; lvl++)
        {
            const auto variable_tuple = compute_pr_variables(ntk, layout, node2pos, lvl);
            const auto orientation    = std::get<0>(variable_tuple);
            const auto gap            = std::get<1>(variable_tuple);
            const auto max_gap        = std::get<2>(variable_tuple);

            ntk.foreach_node_in_rank(lvl,
                                     [this, &layout, &pi2node, &node2pos, &orientation, &gap, &max_gap, &first_pos, &prec_pos](const auto& n, const auto& i)
                                     {
                                         if (!ntk.is_constant(n))
                                         {
                                             // if node is a PI, move it to its correct position
                                             if (ntk.is_pi(n))
                                             {
                                                 if (ntk.rank_position(n) == 0)
                                                 {
                                                     node2pos[n] = layout.move_node(pi2node[n], first_pos);
                                                     prec_pos    = first_pos;
                                                 }
                                                 else
                                                 {
                                                     prec_pos = {prec_pos.x - 1, prec_pos.y + 1};
                                                     // node2pos[n] = layout.move_node(pi2node[n], prec_pos);
                                                     if (prec_pos.x == 0)
                                                     {
                                                         node2pos[n] = layout.move_node(pi2node[n], prec_pos);
                                                     }
                                                     else if (prec_pos.x < (first_pos.x / 2))
                                                     {
                                                         node2pos[n] = layout.move_node(pi2node[n], {0, prec_pos.y});

                                                         node2pos[n] = layout.create_buf(
                                                             wire_east(layout, {0, prec_pos.y}, prec_pos), prec_pos);
                                                     }
                                                     else
                                                     {
                                                         node2pos[n] = layout.move_node(pi2node[n], {prec_pos.x, 0});

                                                         node2pos[n] = layout.create_buf(
                                                             wire_south(layout, {prec_pos.x, 0}, prec_pos), prec_pos);
                                                     }
                                                 }
                                             }
                                             // if n has only one fanin
                                             else if (const auto fc = fanins(ntk, n); fc.fanin_nodes.size() == 1)
                                             {
                                                 const auto& pre       = fc.fanin_nodes[0];
                                                 auto        pre_t     = static_cast<tile<Lyt>>(node2pos[pre]);
                                                 // horizontal (corresponding to colored east)
                                                 if (ntk.rank_position(n) == 0)
                                                 {
                                                     if (orientation[i] == 1)
                                                     {
                                                         first_pos = {first_pos.x, first_pos.y + 1};
                                                         if (first_pos.x > pre_t.x && first_pos.y > pre_t.y)
                                                         {
                                                             pre_t = static_cast<tile<Lyt>>(
                                                                 wire_south(layout, pre_t, {first_pos.x, first_pos.y + 1}));
                                                         }
                                                     }
                                                     else
                                                     {
                                                         first_pos = {first_pos.x + 1, first_pos.y};
                                                     }

                                                     prec_pos    = first_pos;
                                                     node2pos[n] = connect_and_place(layout, first_pos, ntk, n, pre_t);
                                                 }
                                                 else
                                                 {
                                                     prec_pos = {prec_pos.x - 1, prec_pos.y + 1};

                                                     if (prec_pos.x > pre_t.x && prec_pos.y > pre_t.y)
                                                     {
                                                         pre_t =
                                                             static_cast<tile<Lyt>>(wire_south(layout, pre_t, {prec_pos.x, prec_pos.y + 1}));
                                                     }

                                                     node2pos[n] = connect_and_place(layout, prec_pos, ntk, n, pre_t);
                                                 }
                                             }
                                             else  // if node has two fanins (or three fanins with one of them being constant)
                                             {
                                                 const auto &pre1 = fc.fanin_nodes[0], pre2 = fc.fanin_nodes[1];

                                                 auto pre1_t = static_cast<tile<Lyt>>(node2pos[pre1]),
                                                      pre2_t = static_cast<tile<Lyt>>(node2pos[pre2]);

                                                 // pre1_t is the northwards/eastern tile.
                                                 if (pre2_t.y < pre1_t.y)
                                                 {
                                                     std::swap(pre1_t, pre2_t);
                                                 }

                                                 prec_pos = {pre1_t.x, pre2_t.y};

                                                 node2pos[n] =
                                                     connect_and_place(layout, prec_pos, ntk, n, pre1_t, pre2_t, fc.constant_fanin);

                                                 // handle buffering for extra diagonals
                                                 if ((max_gap + 1) > (pre1_t.x - pre2_t.x))
                                                 {
                                                     prec_pos.x += (max_gap + 1 - (pre1_t.x - pre2_t.x));
                                                     node2pos[n] = wire_east(layout, prec_pos, {prec_pos.x + 1, prec_pos.y});
                                                 }

                                                 if (ntk.rank_position(n) == 0)
                                                 {
                                                     first_pos = prec_pos;
                                                 }
                                             }
                                         }
                                     });
        }

        layout.resize({first_pos.x + 1, prec_pos.y + 1, 0});

        layout.resize({10, 10, 0});

        // restore possibly set signal names
        // restore_names(ntk, layout, node2pos);

        // statistical information
        pst.x_size    = layout.x() + 1;
        pst.y_size    = layout.y() + 1;
        pst.num_gates = layout.num_gates();
        pst.num_wires = layout.num_wires();

        return layout;
    }

  private:
    mockturtle::names_view<mockturtle::fanout_view<Ntk>> ntk;

    orthogonal_physical_design_params ps;
    orthogonal_physical_design_stats& pst;

    uint32_t po_counter{0};
};

}  // namespace detail

/**
 * Description
 */
template <typename Lyt, typename Ntk>
Lyt orthogonal_planar_v2(const Ntk& ntk, orthogonal_physical_design_params ps = {},
                         orthogonal_physical_design_stats* pst = nullptr)
{
    static_assert(is_gate_level_layout_v<Lyt>, "Lyt is not a gate-level layout");
    static_assert(mockturtle::is_network_type_v<Ntk>,
                  "Ntk is not a network type");  // Ntk is being converted to a topology_network anyway, therefore,
                                                 // this is the only relevant check here

    // check for input degree
    if (has_high_degree_fanin_nodes(ntk, 2))
    {
        throw high_degree_fanin_exception();
    }

    // check for planarity
    if (!check_planarity(ntk))
    {
        throw std::runtime_error("Input network has to be planar");
    }

    orthogonal_physical_design_stats            st{};
    detail::orthogonal_planar_v2_impl<Lyt, Ntk> p{ntk, ps, st};

    auto result = p.run();

    if (pst)
    {
        *pst = st;
    }

    return result;
}

}  // namespace fiction
#endif  // FICTION_ORTHOGONAL_PLANAR_V2_HPP
