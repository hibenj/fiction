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
uint64_t calculate_gap(const Ntk& ntk, std::vector<uint64_t>& gap,
                       mockturtle::node_map<mockturtle::signal<Lyt>, Ntk> node2pos, uint32_t lvl)
{
    uint64_t max_nl = 0;
    ntk.foreach_node_in_rank(lvl,
                             [&ntk, &gap, &node2pos, &max_nl](const auto& n, const auto& i)
                             {
                                 auto fc = fanins(ntk, n);
                                 // calculate gaps due to AND gates
                                 if (fc.fanin_nodes.size() == 2)
                                 {
                                     std::sort(fc.fanin_nodes.begin(), fc.fanin_nodes.end(), [&ntk](int a, int b)
                                               { return ntk.rank_position(a) < ntk.rank_position(b); });
                                     // compute the max_gap for two fan-ins of  anode
                                     const auto &pre1 = fc.fanin_nodes[0], &pre2 = fc.fanin_nodes[1];

                                     auto pre1_t = static_cast<tile<Lyt>>(node2pos[pre1]),
                                          pre2_t = static_cast<tile<Lyt>>(node2pos[pre2]);

                                     max_nl = std::max(max_nl, static_cast<uint64_t>(pre2_t.y - pre1_t.y));
                                     if (i != 0)
                                     {
                                         gap[i - 1] += 1;
                                     }
                                 }
                             });
    return max_nl;
}

template <typename Ntk, typename Lyt>
std::tuple<std::vector<uint8_t>, std::vector<uint64_t>, uint64_t>
compute_pr_variables(const Ntk& ntk, const Lyt& lyt, mockturtle::node_map<mockturtle::signal<Lyt>, Ntk> node2pos,
                     uint32_t lvl)
{
    std::vector<uint8_t>  orientation(ntk.rank_width(lvl));
    std::vector<uint64_t> new_lines(ntk.rank_width(lvl));
    // calculate the start gaps
    std::vector<uint64_t> gap(ntk.rank_width(lvl));
    uint64_t              max_new_lines = calculate_gap<Ntk, Lyt>(ntk, gap, node2pos, lvl);

    ntk.foreach_node_in_rank(lvl,
                             [&ntk, &node2pos, &orientation, &gap, &new_lines](const auto& n, const auto& i)
                             {
                                 // when there is a node with two fan-ins, we need to add empty diagonals for each empty
                                 // tile between the fan-in nodes (max_gap)
                                 if (ntk.fanin_size(n) == 2)
                                 {
                                     if (i != 0)
                                     {
                                         if (orientation[i - 1] == 1 && gap[i - 1] == 0)
                                         {
                                             orientation[i] = 1;
                                         }
                                     }
                                     std::cout << "And\n";
                                 }
                                 else if (ntk.fanin_size(n) == 1)
                                 {
                                     if (ntk.is_fanout(n))
                                     {
                                         // order the POs
                                         auto fo = ntk.fanout(n);
                                         assert(fo.size() == 2);
                                         std::sort(fo.begin(), fo.end(), [&ntk](int a, int b)
                                                   { return ntk.rank_position(a) < ntk.rank_position(b); });

                                         // if this condition is met, the fan-out does not connect to another node in
                                         // the next level
                                         if (ntk.fanin_size(fo[0]) == 1)
                                         {
                                             if (i != 0)
                                             {
                                                 if (orientation[i - 1] == 0 && gap[i] == 0)
                                                 {
                                                     orientation[i] = 1;
                                                 }
                                                 if (orientation[i - 1] == 1)
                                                 {
                                                     // in all cases there has to be a gap to allow routing
                                                     gap[i - 1] = 1;

                                                     if (gap[i] == 0)
                                                     {
                                                         // add a new line to compensate the gap
                                                         new_lines[i] += 1;
                                                     }
                                                     if (gap[i] < 2)
                                                     {
                                                         // orientation is still 1
                                                         orientation[i] = 1;
                                                     }
                                                     if (gap[i] > 0)
                                                     {
                                                         // use available gap to route the fan-out
                                                         gap[i] -= 1;
                                                     }
                                                 }
                                             }
                                             std::cout << "Non connecting fanout\n";
                                         }
                                         if (ntk.fanin_size(fo[1]) == 1)
                                         {
                                             if (gap[i] == 0)
                                             {
                                                 orientation[i] = 1;
                                             }
                                             std::cout << "Non connecting fanout\n";
                                         }
                                     }
                                     else
                                     {
                                         std::cout << "Buffer\n";
                                         if (i != 0)
                                         {
                                             orientation[i] = orientation[i - 1];
                                         }
                                     }
                                 }
                             });

    max_new_lines  = std::max(max_new_lines, static_cast<uint64_t>(std::accumulate(gap.cbegin(), gap.cend(), 0)));
    const auto ret = std::make_tuple(orientation, gap, max_new_lines);
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

            ntk.foreach_node_in_rank(
                lvl,
                [this, &layout, &pi2node, &node2pos, &orientation, &gap, &max_gap, &first_pos, &prec_pos](const auto& n,
                                                                                                          const auto& i)
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

                                    node2pos[n] =
                                        layout.create_buf(wire_east(layout, {0, prec_pos.y}, prec_pos), prec_pos);
                                }
                                else
                                {
                                    node2pos[n] = layout.move_node(pi2node[n], {prec_pos.x, 0});

                                    node2pos[n] =
                                        layout.create_buf(wire_south(layout, {prec_pos.x, 0}, prec_pos), prec_pos);
                                }
                            }
                        }
                        // if n has only one fanin
                        else if (const auto fc = fanins(ntk, n); fc.fanin_nodes.size() == 1)
                        {
                            const auto& pre   = fc.fanin_nodes[0];
                            auto        pre_t = static_cast<tile<Lyt>>(node2pos[pre]);
                            // horizontal (corresponding to colored east)

                            if (ntk.rank_position(n) == 0)
                            {
                                prec_pos.y  = pre_t.y;
                                prec_pos.x  = pre_t.x + 1;
                                first_pos   = prec_pos;
                                node2pos[n] = connect_and_place(layout, first_pos, ntk, n, pre_t);
                            }
                            else
                            {
                                // compare them with the node placed with rank(n) - 1
                               /* const auto& pre_check = ntk.at_rank_position(ntk.rank_position(n) - 1);
                                auto        pre_check_t = static_cast<tile<Lyt>>(node2pos[pre_check]);

                                prec_pos;*/

                                prec_pos.x -= gap[i-1];
                                prec_pos.y += gap[i-1];

                                tile<Lyt> t = pre_t;
                                t.x += 1;

                                // if it can be wired east, wire east
                                if (prec_pos != t)
                                {
                                    node2pos[n] = connect_and_place(layout, t, ntk, n, pre_t);
                                }
                                // if it can not be wired east then it has to be wired south
                                else
                                {
                                    t = pre_t;
                                    t.y += 1;
                                    node2pos[n] = connect_and_place(layout, t, ntk, n, pre_t);
                                }
                                prec_pos = t;
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
                            /*if ((max_gap + 1) > (pre1_t.x - pre2_t.x))
                            {
                                prec_pos.x += (max_gap + 1 - (pre1_t.x - pre2_t.x));
                                node2pos[n] = wire_east(layout, prec_pos, {prec_pos.x + 1, prec_pos.y});
                            }*/

                            if (ntk.rank_position(n) == 0)
                            {
                                first_pos = prec_pos;
                            }
                        }
                    }
                });
        }

        std::unordered_map<int, int> countMap;
        int                          add_line = 0;
        // the number of outputs on a node is limited to 2, due to fanout substitution
        ntk.foreach_po(
            [&](const auto& po)
            {
                if (!ntk.is_constant(po))
                {
                    const auto n_s     = node2pos[po];
                    auto       po_tile = static_cast<tile<Lyt>>(n_s);
                    if (countMap[po] < 2)  // Check if the count is less than 2
                    {
                        // Adjust the position based on whether it's the first or second occurrence
                        if (countMap[po] == 1)
                        {
                            if (po_tile.y == prec_pos.y)
                            {
                                add_line = 1;
                            }
                            po_tile = static_cast<tile<Lyt>>(wire_south(layout, po_tile, {po_tile.x, po_tile.y + 2}));
                        }
                        const tile<Lyt> anker{po_tile};
                        po_tile.x = first_pos.x + 1;

                        // Create PO and increment the count
                        layout.create_po(wire_east(layout, anker, po_tile),
                                         ntk.has_output_name(po_counter) ? ntk.get_output_name(po_counter++) :
                                                                           fmt::format("po{}", po_counter++),
                                         po_tile);
                        countMap[po]++;
                    }
                    else
                    {
                        assert(false);
                    }
                }
            });

        layout.resize({first_pos.x + 1, prec_pos.y + add_line, 0});

        // layout.resize({10, 10, 0});

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
