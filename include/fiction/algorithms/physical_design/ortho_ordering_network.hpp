//
// Created by benjamin on 11.04.23.
//

#ifndef FICTION_ORTHO_ORDERING_NETWORK_HPP
#define FICTION_ORTHO_ORDERING_NETWORK_HPP

#include "fiction/algorithms/network_transformation/inverter_substitution.hpp"
#include "fiction/networks/views/input_ordering_view.hpp"
#include "orthogonal.hpp"

namespace fiction
{

namespace detail
{

template <typename Ntk>
void paint_node_and_edges(const coloring_container<Ntk>& ctn, mockturtle::node<Ntk> const& n, uint32_t color)
{
    const auto finc = fanin_edges(ctn.color_ntk, n);
    std::for_each(finc.fanin_edges.cbegin(), finc.fanin_edges.cend(),
                  [&ctn, &color](const auto& fe) { ctn.color_ntk.paint_edge(fe, color); });
    ctn.color_ntk.paint(mockturtle::node<Ntk>{n}, color);
}

template <typename Ntk>
int64_t get_index(std::vector<Ntk> v, Ntk n)
{
    auto it = find(v.begin(), v.end(), n);

    // If element was found
    if (it != v.end())
    {
        // calculating the index
        int64_t index = it - v.begin();
        return index;
    }
    return -1;
}

/*
 * Conditional coloring rules:
 * 0. All 1-fan-in nodes (inverters and fan-outs) after PIs have to be colored east
 * 1. fan-out node related to two PIs:
 * - Case: 2-inverters -> get balanced to the beginning of the fan-out node (nl_inv has to be respected here for
 * instance)
 * - Case: one inverter (color inverter east and other outgoing edges south)
 * - Case: no inverter (color one outgoing edge east and one south)
 * 2. fan-out node related to one PI (coloring is dependent on the coloring of the whole network)
 * 3. PI connected to node connected to PI: color connecting node east
 */
template <typename Ntk>
coloring_container<Ntk> conditional_coloring(const Ntk& ntk) noexcept
{
    coloring_container<Ntk> ctn{ntk};

    // currently viewed node
    mockturtle::node<Ntk> current_node;

#if (PROGRESS_BARS)
    // initialize a progress bar
    mockturtle::progress_bar bar{static_cast<uint32_t>(ctn.color_ntk.num_gates()),
                                 "[i] determining relative positions: |{0}|"};
#endif

    // Find a coloring
    ntk.foreach_gate_reverse(
        [&](const auto& n, [[maybe_unused]] const auto i)
        {
            const auto finc = fanin_edges(ctn.color_ntk, n);

            // if any incoming edge is colored east, color them all east, and south otherwise
            const auto color =
                std::any_of(finc.fanin_edges.cbegin(), finc.fanin_edges.cend(),
                            [&ctn](const auto& fe) { return ctn.color_ntk.edge_color(fe) == ctn.color_east; }) ?
                    ctn.color_east :
                    ctn.color_south;

            std::for_each(finc.fanin_edges.cbegin(), finc.fanin_edges.cend(),
                          [&ctn, &color](const auto& fe) { recursively_paint_edges(ctn, fe, color); });

            // if all incoming edges are colored east, paint the node east as well
            if (std::all_of(finc.fanin_edges.cbegin(), finc.fanin_edges.cend(),
                            [&ctn](const auto& fe) { return ctn.color_ntk.edge_color(fe) == ctn.color_east; }))
            {
                ctn.color_ntk.paint(mockturtle::node<Ntk>{n}, ctn.color_east);
            }
            // else, if all incoming edges are colored south, paint the node south as well
            else if (std::all_of(finc.fanin_edges.cbegin(), finc.fanin_edges.cend(),
                                 [&ctn](const auto& fe) { return ctn.color_ntk.edge_color(fe) == ctn.color_south; }))
            {
                ctn.color_ntk.paint(mockturtle::node<Ntk>{n}, ctn.color_south);
            }

#if (PROGRESS_BARS)
            // update progress
            bar(i);
#endif
        });

    // Adjust only the nodes viewed in the ordering network
    ntk.foreach_pi(
        [&](const auto& pi)
        {
            ntk.foreach_fanout(
                pi,
                [&](const auto& fon)
                {
                    // Always track the current_node
                    current_node = fon;

                    // Skip Inverters and color them east
                    if (ntk.is_inv(current_node))
                    {
                        // Color Inverter east
                        paint_node_and_edges(ctn, current_node, ctn.color_east);
                        auto cur_fon = fanouts(ctn.color_ntk, current_node);
                        // Jump to next node
                        current_node = cur_fon[0];
                    }
                    // 1. fan-out node related to two PIs
                    if (ntk.is_fanout(current_node) && ntk.fanout_size(current_node) >= 2)
                    {
                        paint_node_and_edges(ctn, current_node, ctn.color_east);

                        /*bool inv_flag = false;*/
                        if (auto fo_two = ctn.color_ntk.get_fo_two(); get_index(fo_two, pi) % 3 == 0)
                        {
                            auto cur_fon = fanouts(ctn.color_ntk, current_node);
                            /*// Jump to next node
                            current_node = cur_fon[0];
                            if (ntk.is_inv(current_node))
                            {
                                // Color Inverter east
                                paint_node_and_edges(ctn, current_node, ctn.color_east);
                                inv_flag = true;
                            }*/

                            auto swap_color = ctn.color_south;
                            ntk.foreach_fanout(
                                current_node,
                                [&ctn, &current_node, &ntk, &fon, &swap_color](const auto& cur_fon)
                                {
                                    // Jump to next node
                                    current_node = cur_fon;

                                    if (ntk.is_inv(current_node))
                                    {
                                        // Color Inverter east
                                        paint_node_and_edges(ctn, current_node, ctn.color_east);
                                        ntk.foreach_fanout(fon,
                                                           [&](const auto& fon_inv)
                                                           {
                                                               // Jump to next node
                                                               current_node = fon_inv;
                                                           });
                                        paint_node_and_edges(ctn, current_node, ctn.color_south);
                                    }
                                    /*else if (inv_flag)
                                    {
                                        paint_node_and_edges(ctn, current_node, ctn.color_south);
                                    }*/
                                    else
                                    {
                                        paint_node_and_edges(ctn, current_node, swap_color);
                                        swap_color = ctn.color_east;
                                    }
                                });
                        }
                    }
                    // 2. fan-out node related to one PI
                    else if (auto fo_one = ctn.color_ntk.get_fo_one(); get_index(fo_one, pi) % 2 == 1)
                    {
                        const auto fc = fanins(ctn.color_ntk, current_node);
                        if (ntk.is_inv(fc.fanin_nodes[0]))
                        {
                            const auto fc_fi = fanins(ctn.color_ntk, fc.fanin_nodes[0]);
                            if (ntk.is_fanout(fc_fi.fanin_nodes[0]) && ntk.fanout_size(fc_fi.fanin_nodes[0]) >= 2)
                            {
                                if (ctn.color_ntk.color(fc.fanin_nodes[0]) == ctn.color_south)
                                {
                                    paint_node_and_edges(ctn, current_node, ctn.color_east);
                                }
                                else
                                {
                                    paint_node_and_edges(ctn, current_node, ctn.color_south);
                                }
                            }
                        }
                        else if (ntk.is_inv(fc.fanin_nodes[1]))
                        {
                            const auto fc_fi = fanins(ctn.color_ntk, fc.fanin_nodes[1]);
                            if (ntk.is_fanout(fc_fi.fanin_nodes[0]) && ntk.fanout_size(fc_fi.fanin_nodes[0]) >= 2)
                            {
                                if (ctn.color_ntk.color(fc.fanin_nodes[1]) == ctn.color_south)
                                {
                                    paint_node_and_edges(ctn, current_node, ctn.color_east);
                                }
                                else
                                {
                                    paint_node_and_edges(ctn, current_node, ctn.color_south);
                                }
                            }
                        }
                    }
                    // 3. PI connected to node connected to PI
                    else if (auto pi_pi = ctn.color_ntk.get_pi_to_pi(); get_index(pi_pi, pi) % 2 == 0)
                    {
                        if (const auto fc = fanins(ctn.color_ntk, current_node);
                            (ntk.is_inv(fc.fanin_nodes[0]) && !ntk.is_inv(fc.fanin_nodes[1])) ||
                            (!ntk.is_inv(fc.fanin_nodes[0]) && ntk.is_inv(fc.fanin_nodes[1])))
                        {
                            paint_node_and_edges(ctn, current_node, ctn.color_south);
                        }
                        else
                        {
                            paint_node_and_edges(ctn, current_node, ctn.color_east);
                        }
                    }
                });
        });

    return ctn;
}

template <typename Lyt, typename Ntk>
class orthogonal_ordering_network_impl
{
  public:
    orthogonal_ordering_network_impl(const Ntk& src, const orthogonal_physical_design_params& p,
                                     orthogonal_physical_design_stats& st) :
            ntk{input_ordering_view{mockturtle::fanout_view{
                inverter_substitution(fanout_substitution<mockturtle::names_view<technology_network>>(src))}}},
            ps{p},
            pst{st}
    {}

    Lyt run()
    {
        // measure run time
        mockturtle::stopwatch stop{pst.time_total};
        // compute coloring
        const auto ctn = conditional_coloring(ntk);

        mockturtle::node_map<mockturtle::signal<Lyt>, decltype(ctn.color_ntk)> node2pos{ctn.color_ntk};

        // instantiate the layout
        Lyt layout{{1, 1, 1}, twoddwave_clocking<Lyt>(ps.number_of_clock_phases)};

        // reserve PI nodes without positions
        auto pi2node = reserve_input_nodes(layout, ctn.color_ntk);

        // first x-pos to use for gates is 1 because PIs take up the 0th column
        tile<Lyt> latest_pos{1, 0};
        // ordering network tracker
        tile<Lyt> latest_pos_inputs{0, 0};

        // Reserve new columns for inverters in the ordering network
        if (ctn.color_ntk.nc_inv_flag())
        {
            latest_pos.x = latest_pos.x + ctn.color_ntk.nc_inv_num() + 1;
        }
        // new column inverter tracker
        std::uint64_t insert_position_inv{1};

#if (PROGRESS_BARS)
        // initialize a progress bar
        mockturtle::progress_bar bar{static_cast<uint32_t>(ctn.color_ntk.size()), "[i] arranging layout: |{0}|"};
#endif
        // Find multi_output_nodes
        std::vector<mockturtle::node<Ntk>> my_out_nodes;
        std::vector<mockturtle::node<Ntk>> multi_out_nodes;
        ctn.color_ntk.foreach_po(
            [&](const auto& po)
            {
                if (std::find(my_out_nodes.begin(), my_out_nodes.end(), po) != my_out_nodes.end())
                {
                    multi_out_nodes.push_back(po);
                }
                my_out_nodes.push_back(po);
            });

        // Start of the algorithm
        ntk.foreach_node(
            [&, this](const auto& n, [[maybe_unused]] const auto i)
            {
                // do not place constants
                if (!ctn.color_ntk.is_constant(n))
                {
                    // if node is a PI, move it to its correct position
                    if (ctn.color_ntk.is_pi(n))
                    {
                        node2pos[n] = layout.move_node(pi2node[n], {latest_pos_inputs});
                        ++latest_pos_inputs.y;
                    }
                    // if n has only one fan-in
                    else if (const auto fc = fanins(ctn.color_ntk, n); fc.fanin_nodes.size() == 1)
                    {
                        const auto& pre = fc.fanin_nodes[0];

                        const auto pre_t = static_cast<tile<Lyt>>(node2pos[pre]);

                        // n is colored east
                        if (const auto clr = ctn.color_ntk.color(n); clr == ctn.color_east)
                        {
                            // new column for inverters
                            auto insert_position = latest_pos.x;
                            if (ntk.nc_inv_flag() && ntk.is_inv(n) && ntk.is_pi(pre))
                            {
                                insert_position = insert_position_inv;
                                ++insert_position_inv;
                                --latest_pos.x;
                            }

                            if (ctn.color_ntk.is_fanout(n) && ctn.color_ntk.is_pi(pre))
                            {
                                ++latest_pos.y;
                            }
                            const tile<Lyt> t{insert_position, pre_t.y};

                            node2pos[n] = connect_and_place(layout, t, ctn.color_ntk, n, pre_t);
                            ++latest_pos.x;
                        }
                        // n is colored south
                        else if (clr == ctn.color_south)
                        {
                            if ((ctn.color_ntk.is_inv(n) || ctn.color_ntk.is_fanout(n)) &&
                                latest_pos.y < latest_pos_inputs.y)
                            {
                                const tile<Lyt> t{pre_t.x, latest_pos_inputs.y};

                                node2pos[n]  = connect_and_place(layout, t, ctn.color_ntk, n, pre_t);
                                latest_pos.y = t.y + 1;
                            }
                            else
                            {
                                const tile<Lyt> t{pre_t.x, latest_pos.y};

                                node2pos[n] = connect_and_place(layout, t, ctn.color_ntk, n, pre_t);
                                ++latest_pos.y;
                            }
                        }
                        else
                        {
                            // single fan-in nodes should not be colored null
                            assert(false);
                        }
                    }
                    else  // if node has two fan-ins (or three fan-ins with one of them being constant)
                    {
                        const auto &pre1 = fc.fanin_nodes[0], pre2 = fc.fanin_nodes[1];

                        auto pre1_t = static_cast<tile<Lyt>>(node2pos[pre1]),
                             pre2_t = static_cast<tile<Lyt>>(node2pos[pre2]);

                        tile<Lyt> t{};

                        // n is colored east
                        if (const auto clr = ctn.color_ntk.color(n); clr == ctn.color_east)
                        {
                            // make sure pre1_t is the northward tile
                            if (pre2_t.y < pre1_t.y)
                            {
                                std::swap(pre1_t, pre2_t);
                            }

                            // use larger y position of predecessors
                            t = {latest_pos.x, pre2_t.y};

                            // each 2-input gate has one incoming bent wire
                            pre1_t = static_cast<tile<Lyt>>(wire_east(layout, pre1_t, {t.x + 1, pre1_t.y}));

                            ++latest_pos.x;
                            if (latest_pos.y < latest_pos_inputs.y)
                            {
                                latest_pos.y = t.y + 1;
                            }
                        }
                        // n is colored south
                        else if (clr == ctn.color_south)
                        {
                            // instantiate the PIs affected by the ordering
                            auto fo_two = ctn.color_ntk.get_fo_two();
                            auto fo_one = ctn.color_ntk.get_fo_one();
                            auto pi_pi  = ctn.color_ntk.get_pi_to_pi();
                            // resolve conflicts for PIs not affected by the ordering
                            if (ctn.color_ntk.is_pi(pre2) &&
                                std::find(fo_two.begin(), fo_two.end(), pre2) == fo_two.end() &&
                                std::find(fo_one.begin(), fo_one.end(), pre2) == fo_one.end() &&
                                std::find(pi_pi.begin(), pi_pi.end(), pre2) == pi_pi.end())
                            {
                                pre2_t =
                                    static_cast<tile<Lyt>>(wire_east(layout, pre2_t, {latest_pos.x + 1, pre2_t.y}));
                                ++latest_pos.x;
                            }
                            if (ctn.color_ntk.is_pi(pre1) &&
                                std::find(fo_two.begin(), fo_two.end(), pre1) == fo_two.end() &&
                                std::find(fo_one.begin(), fo_one.end(), pre1) == fo_one.end() &&
                                std::find(pi_pi.begin(), pi_pi.end(), pre1) == pi_pi.end())
                            {
                                pre1_t =
                                    static_cast<tile<Lyt>>(wire_east(layout, pre1_t, {latest_pos.x + 1, pre1_t.y}));
                                ++latest_pos.x;
                            }
                            auto pre_fo = pre2;
                            if (pre2_t.x > pre1_t.x)
                            {
                                std::swap(pre1_t, pre2_t);
                                pre_fo = pre1;
                            }
                            // Area saving south rule
                            // check if pre1_t is now also the northward tile
                            if (pre1_t.y < pre2_t.y && !ctn.color_ntk.is_fanout(pre_fo))
                            {
                                if (pre2_t.x == pre1_t.x)
                                {
                                    // use larger x position of predecessors
                                    t = {latest_pos.x, pre2_t.y};
                                    ++latest_pos.x;
                                }
                                else
                                {
                                    // use larger x position of predecessors
                                    t = {pre1_t.x, pre2_t.y};
                                }
                                if (pre2_t.y + 1 > latest_pos.y)
                                {
                                    latest_pos.y = pre2_t.y + 1;
                                }
                            }
                            /**!!**************************************************************************************/
                            else
                            {
                                if (latest_pos.y < latest_pos_inputs.y)
                                {
                                    // use larger x position of predecessors
                                    t = {pre1_t.x, latest_pos_inputs.y};
                                    // each 2-input gate has one incoming bent wire
                                    pre2_t = static_cast<tile<Lyt>>(wire_south(layout, pre2_t, {pre2_t.x, t.y + 1}));

                                    latest_pos.y = t.y + 1;
                                }
                                else
                                {
                                    // use larger x position of predecessors
                                    t = {pre1_t.x, latest_pos.y};

                                    // each 2-input gate has one incoming bent wire
                                    pre2_t = static_cast<tile<Lyt>>(wire_south(layout, pre2_t, {pre2_t.x, t.y + 1}));

                                    ++latest_pos.y;
                                }
                            }
                        }
                        // n is colored null; corner case
                        else
                        {
                            // check which output direction is already used
                            auto fos_pre1   = fanouts(ctn.color_ntk, pre1);
                            auto color_pre1 = std::any_of(fos_pre1.begin(), fos_pre1.end(),
                                                          [&ctn](const auto& fe)
                                                          { return ctn.color_ntk.color(fe) == ctn.color_east; }) ?
                                                  ctn.color_south :
                                                  ctn.color_east;

                            auto fos_pre2   = fanouts(ctn.color_ntk, pre2);
                            auto color_pre2 = std::any_of(fos_pre2.begin(), fos_pre2.end(),
                                                          [&ctn](const auto& fe)
                                                          { return ctn.color_ntk.color(fe) == ctn.color_east; }) ?
                                                  ctn.color_south :
                                                  ctn.color_east;

                            t = latest_pos;

                            if (color_pre1 == ctn.color_east && color_pre2 == ctn.color_south)
                            {
                                // both wires have one bent
                                pre1_t = static_cast<tile<Lyt>>(wire_east(layout, pre1_t, {t.x + 1, pre1_t.y}));
                                pre2_t = static_cast<tile<Lyt>>(wire_south(layout, pre2_t, {pre2_t.x, t.y + 1}));
                            }
                            else
                            {
                                // both wires have one bent
                                pre1_t = static_cast<tile<Lyt>>(wire_south(layout, pre1_t, {pre1_t.x, t.y + 1}));
                                pre2_t = static_cast<tile<Lyt>>(wire_east(layout, pre2_t, {t.x + 1, pre2_t.y}));
                            }

                            ++latest_pos.x;
                            ++latest_pos.y;
                        }

                        node2pos[n] = connect_and_place(layout, t, ctn.color_ntk, n, pre1_t, pre2_t, fc.constant_fanin);
                    }
                    if (ctn.color_ntk.is_po(n))
                    {
                        if (!is_eastern_po_orientation_available(ctn, n) ||
                            std::find(multi_out_nodes.begin(), multi_out_nodes.end(), n) != multi_out_nodes.end())
                        {
                            ++latest_pos.y;
                            ++latest_pos_inputs.y;
                        }
                    }
                }

#if (PROGRESS_BARS)
                // update progress
                bar(i);
#endif
            });

        // Since the layout size is only known after placing all gates, the POs are placed after the main algorithm
        bool                               multi_out_node = false;
        std::vector<mockturtle::node<Ntk>> out_nodes;
        ctn.color_ntk.foreach_po(
            [this, &out_nodes, &multi_out_node, &node2pos, &ctn, &layout, &latest_pos](const auto& po)
            {
                const auto n_s = node2pos[po];

                tile<Lyt> po_tile{};

                if (std::find(out_nodes.begin(), out_nodes.end(), po) != out_nodes.end())
                {
                    multi_out_node = true;
                }

                // determine PO orientation
                if (is_eastern_po_orientation_available(ctn, po) && !multi_out_node)
                {
                    po_tile = static_cast<tile<Lyt>>(n_s);
                    layout.resize({latest_pos.x, latest_pos.y - 1, 1});
                }
                else
                {
                    po_tile = static_cast<tile<Lyt>>(n_s);
                    po_tile = static_cast<tile<Lyt>>(wire_south(layout, po_tile, {po_tile.x, po_tile.y + 2}));
                    layout.resize({latest_pos.x, latest_pos.y - 1, 1});
                }
                // check if PO position is located at the border
                if (layout.is_at_eastern_border({po_tile.x + 1, po_tile.y}) && !multi_out_node)
                {
                    ++po_tile.x;
                    layout.create_po(n_s,
                                     ctn.color_ntk.has_output_name(po_counter) ?
                                         ctn.color_ntk.get_output_name(po_counter++) :
                                         fmt::format("po{}", po_counter++),
                                     po_tile);
                }
                // place PO at the border and connect it by wire segments
                else
                {
                    const tile<Lyt> anker{po_tile};

                    po_tile = layout.eastern_border_of(po_tile);

                    layout.create_po(wire_east(layout, anker, po_tile),
                                     ctn.color_ntk.has_output_name(po_counter) ?
                                         ctn.color_ntk.get_output_name(po_counter++) :
                                         fmt::format("po{}", po_counter++),
                                     po_tile);
                    multi_out_node = false;
                }

                out_nodes.push_back(po);
            });

        // restore possibly set signal names
        restore_names(ctn.color_ntk, layout, node2pos);

        // statistical information
        pst.x_size    = layout.x() + 1;
        pst.y_size    = layout.y() + 1;
        pst.num_gates = layout.num_gates();
        pst.num_wires = layout.num_wires();

        return layout;
    }

  private:
    input_ordering_view<mockturtle::fanout_view<mockturtle::names_view<technology_network>>> ntk;

    orthogonal_physical_design_params ps;
    orthogonal_physical_design_stats& pst;

    uint32_t po_counter{0};
};

}  // namespace detail

/**
 * A modification of the orthogonal algorithm, which saves area and wire crossings by ordering PIs
 * and coloring the affected nodes based on the relevant conditions.
 *
 * May throw a high_degree_fanin_exception if `ntk` contains any node with a fan-in larger than 2.
 *
 * @tparam Lyt Desired gate-level layout type.
 * @tparam Ntk Network type that acts as specification.
 * @param ntk The network that is to place and route.
 * @param ps Parameters.
 * @param pst Statistics.
 * @return A gate-level layout of type `Lyt` that implements `ntk` as an FCN circuit.
 */
template <typename Lyt, typename Ntk>
Lyt orthogonal_ordering_network(const Ntk& ntk, orthogonal_physical_design_params ps = {},
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

    orthogonal_physical_design_stats                   st{};
    detail::orthogonal_ordering_network_impl<Lyt, Ntk> p{ntk, ps, st};

    auto result = p.run();

    if (pst)
    {
        *pst = st;
    }

    return result;
}

}  // namespace fiction

#endif  // FICTION_ORTHO_ORDERING_NETWORK_HPP