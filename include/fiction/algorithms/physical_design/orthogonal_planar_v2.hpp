//
// Created by benjamin on 21.01.25.
//

#ifndef FICTION_ORTHOGONAL_PLANAR_V2_HPP
#define FICTION_ORTHOGONAL_PLANAR_V2_HPP

#include "fiction/algorithms/properties/check_planarity.hpp"
#include "fiction/io/print_layout.hpp"
#include "fiction/layouts/clocking_scheme.hpp"
#include "fiction/traits.hpp"
#include "fiction/utils/debug/network_writer.hpp"
#include "fiction/utils/name_utils.hpp"
#include "fiction/utils/network_utils.hpp"
#include "fiction/utils/placement_utils.hpp"

#include <fiction/algorithms/physical_design/orthogonal.hpp>

#include <mockturtle/traits.hpp>
#include <mockturtle/utils/node_map.hpp>
#include <mockturtle/utils/stopwatch.hpp>
#include <mockturtle/views/fanout_view.hpp>
#include <mockturtle/views/rank_view.hpp>

#include <algorithm>
#include <unordered_map>
#include <vector>

#if (PROGRESS_BARS)
#include <mockturtle/utils/progress_bar.hpp>
#endif

namespace fiction
{

namespace detail
{

/**
 * Determines the orientation of the first non-buffer node in a level. This orientation is used to define the
 * orientation of all buffers placed before this node within the same level.
 */
template <typename Ntk>
int start_orientation(Ntk& ntk, uint32_t lvl)
{
    int orientation = 0;
    if (lvl == 0)
    {
        return orientation;
    }
    ntk.foreach_node_in_rank(lvl,
                             [&ntk, &orientation](const auto& n)
                             {
                                 // It is a 2-ary node
                                 if (ntk.fanin_size(n) == 2)
                                 {
                                     orientation = 0;
                                     return orientation;
                                 }
                                 // It is a fan-out of a fan-out node
                                 if (const auto fc = fanins(ntk, n); fc.fanin_nodes.size() == 1)
                                 {
                                     const auto& pre = fc.fanin_nodes[0];
                                     if (ntk.is_fanout(pre) && ntk.fanout_size(pre) == 2)
                                     {
                                         orientation = 1;
                                         return orientation;
                                     }
                                 }
                             });
    return orientation;
}

template <typename Lyt, typename Ntk>
class orthogonal_planar_impl
{
  public:
    orthogonal_planar_impl(const Ntk& src, const orthogonal_physical_design_params& p,
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
            std::size_t r           = 0;
            auto        orientation = start_orientation(ntk, lvl);
            ntk.foreach_node_in_rank(
                lvl,
                [this, &r, &level_gaps, &forward_gap_array, &first_pos, &prec_pos, &node2pos, &pi2node, &layout,
                 &orientation](const auto& n)
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
                                int gap  = level_gaps[r - 1];
                                prec_pos = {prec_pos.x - gap - 1, prec_pos.y + gap + 1};
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
                            const auto& pre       = fc.fanin_nodes[0];
                            auto        pre_t     = static_cast<tile<Lyt>>(node2pos[pre]);
                            const int   new_lines = level_gaps.back();
                            // horizontal (corresponding to colored east)
                            if (ntk.rank_position(n) == 0)
                            {
                                if (orientation == 1)
                                {
                                    first_pos = {first_pos.x + new_lines, first_pos.y + 1};
                                    if (first_pos.x > pre_t.x && first_pos.y > pre_t.y)
                                    {
                                        pre_t = static_cast<tile<Lyt>>(
                                            wire_south(layout, pre_t, {first_pos.x, first_pos.y + 1}));
                                    }
                                }
                                else
                                {
                                    first_pos = {first_pos.x + new_lines + 1, first_pos.y};
                                }

                                prec_pos    = first_pos;
                                node2pos[n] = connect_and_place(layout, first_pos, ntk, n, pre_t);
                            }
                            else
                            {
                                int gap  = level_gaps[r - 1];
                                prec_pos = {prec_pos.x - gap - 1, prec_pos.y + gap + 1};

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

                            // only for checking correctness
                            tile<Lyt> check_pos = prec_pos;

                            prec_pos = {pre1_t.x, pre2_t.y};

                            node2pos[n] =
                                connect_and_place(layout, prec_pos, ntk, n, pre1_t, pre2_t, fc.constant_fanin);

                            // and nodes always get buffered east for new lines
                            const int new_lines = level_gaps.back();
                            tile<Lyt> t         = prec_pos;
                            if ((new_lines + 1) > (pre1_t.x - pre2_t.x))
                            {
                                prec_pos.x += (new_lines + 1 - (pre1_t.x - pre2_t.x));
                                node2pos[n] = wire_east(layout, t, {prec_pos.x + 1, prec_pos.y});
                            }

                            if (ntk.rank_position(n) == 0)
                            {
                                first_pos = prec_pos;
                            }
                            else
                            {
                                int gap   = level_gaps[r - 1];
                                check_pos = {check_pos.x - gap - 1, check_pos.y + gap + 1};
                                assert(prec_pos == check_pos);
                            }
                        }
                    }
                    ++r;
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

        // restore possibly set signal names
        restore_names(ntk, layout, node2pos);

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
Lyt orthogonal_planar(const Ntk& ntk, orthogonal_physical_design_params ps = {},
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

    orthogonal_physical_design_stats         st{};
    detail::orthogonal_planar_impl<Lyt, Ntk> p{ntk, ps, st};

    auto result = p.run();

    if (pst)
    {
        *pst = st;
    }

    return result;
}

}  // namespace fiction
#endif  // FICTION_ORTHOGONAL_PLANAR_V2_HPP
