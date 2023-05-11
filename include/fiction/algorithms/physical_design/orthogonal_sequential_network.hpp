//
// Created by benjamin on 02.05.23.
//

#ifndef FICTION_ORTHOGONAL_SEQUENTIAL_NETWORK_HPP
#define FICTION_ORTHOGONAL_SEQUENTIAL_NETWORK_HPP
#include "fiction/networks/sequential_technology_network.hpp"
#include "fiction/networks/technology_network.hpp"
#include "orthogonal.hpp"
#include "orthogonal_majority_network.hpp"

#if (PROGRESS_BARS)
#include <mockturtle/utils/progress_bar.hpp>
#endif

namespace fiction
{
namespace detail
{

template <typename Lyt, typename Ntk>
aspect_ratio<Lyt> determine_sequential_layout_size(const coloring_container<Ntk>& ctn) noexcept
{
#if (PROGRESS_BARS)
    // initialize a progress bar
    mockturtle::progress_bar bar{static_cast<uint32_t>(ctn.color_ntk.size()), "[i] determining layout size: |{0}|"};
#endif
    auto     new_field_cis = floor((ctn.color_ntk.num_cis() - 1) / 4);
    uint64_t x             = 0ull +
                 static_cast<uint64_t>((new_field_cis * 2) + (ctn.color_ntk.num_registers() - 1) * 4 +
                                       ctn.color_ntk.num_registers() * 2) +
                 1,
             y = ctn.color_ntk.num_pis() + ctn.color_ntk.num_registers() * 3 - 1;
    int test = ctn.color_ntk.num_registers();
    ctn.color_ntk.foreach_node(
        [&](const auto& n, [[maybe_unused]] const auto i)
        {
            if (!ctn.color_ntk.is_constant(n))
            {
                if (ctn.color_ntk.is_pi(n))
                {
                    ctn.color_ntk.foreach_fanout(n,
                                                 [&ctn, &x](const auto& fon)
                                                 {
                                                     if (ctn.color_ntk.color(fon) == ctn.color_south)
                                                     {
                                                         ++x;
                                                     }
                                                 });
                }
                else if (ctn.color_ntk.is_ro(n))
                {
                    ctn.color_ntk.foreach_fanout(n,
                                                 [&ctn, &x](const auto& fon)
                                                 {
                                                     if (ctn.color_ntk.color(fon) == ctn.color_south)
                                                     {
                                                         ++x;
                                                     }
                                                 });
                }
                else if (const auto clr = ctn.color_ntk.color(n); clr == ctn.color_east)
                {
                    ++x;
                }
                else if (clr == ctn.color_south)
                {
                    ++y;
                }
                else if (clr == ctn.color_null)
                {
                    ++x;
                    ++y;
                }

                if (ctn.color_ntk.is_po(n))
                {
                    if (!is_eastern_po_orientation_available(ctn, n))
                    {
                        ++y;
                    }
                }
                if (ctn.color_ntk.is_ri(n))
                {
                    if (is_eastern_po_orientation_available(ctn, n))
                    {
                        ++x;
                    }
                    else
                    {
                        ++y;
                        ++x;
                    }
                }
            }

#if (PROGRESS_BARS)
            // update progress
            bar(i);
#endif
        });

    return {x, y, 1};
}

template <typename Ntk, typename Lyt>
void wire_registers(Lyt layout, const coloring_container<Ntk>& ctn, tile<Lyt>& ri_tile, const uint32_t& num_ris,
                    const uint32_t& reg_number, aspect_ratio<Lyt> aspect_ratio,
                    const mockturtle::node_map<mockturtle::signal<Lyt>, decltype(ctn.color_ntk)>& node2pos,
                    const mockturtle::node<Ntk>                                                   n)
{
    ri_tile =
        static_cast<tile<Lyt>>(wire_south(layout, ri_tile, {ri_tile.x, aspect_ratio.y + 2 - 2 * num_ris + reg_number}));

    // general solution for N registers
    const auto ro_position    = node2pos[ctn.color_ntk.ri_to_ro(n)];
    const auto ri_tile_ro_pos = static_cast<tile<Lyt>>(ro_position);

    const auto global_syc_const = floor((ri_tile_ro_pos.y) / 4);
    ri_tile                     = static_cast<tile<Lyt>>(
        wire_east(layout, ri_tile, {ri_tile.x + num_ris + reg_number * 3 + global_syc_const * 2 + 1, ri_tile.y}));

    ri_tile = static_cast<tile<Lyt>>(
        wire_south(layout, ri_tile, {ri_tile.x, ri_tile.y + (num_ris - 1 - reg_number) * 2 + 2}));

    auto pre_clock = layout.get_clock_number({ri_tile});

    const uint32_t reg_pos_x = 2 * num_ris;

    if ((ri_tile.y - static_cast<tile<Lyt>>(node2pos[ctn.color_ntk.ro_at(reg_number)]).y) % 2 !=
        (reg_pos_x - ri_tile.x) % 2)
    {
        for (auto index = ri_tile.x - 1; index > reg_pos_x - 3 - 2 * (num_ris - reg_number - 1) + 1; --index)
        {
            layout.assign_clock_number({index, ri_tile.y, 0}, pre_clock + 1);
            layout.assign_clock_number({index, ri_tile.y, 1}, pre_clock + 1);
            pre_clock = layout.get_clock_number({index, ri_tile.y});
        }
        ri_tile = static_cast<tile<Lyt>>(
            wire_west(layout, ri_tile, {reg_pos_x - 3 - 2 * (num_ris - reg_number - 1) + 1, ri_tile.y}));
    }
    else
    {
        if ((reg_pos_x - 2 * (num_ris - reg_number - 1)) < 3)
        {
            for (auto index = ri_tile.x - 1; index > reg_pos_x - 3 - 2 * (num_ris - reg_number - 1) + 1; --index)
            {
                layout.assign_clock_number({index, ri_tile.y, 0}, pre_clock + 1);
                layout.assign_clock_number({index, ri_tile.y, 1}, pre_clock + 1);
                pre_clock = layout.get_clock_number({index, ri_tile.y});
            }
            layout.assign_clock_number({0, ri_tile.y, 0}, pre_clock + 1);
            layout.assign_clock_number({0, ri_tile.y, 1}, pre_clock + 1);
            ri_tile = static_cast<tile<Lyt>>(
                wire_west(layout, ri_tile, {reg_pos_x - 3 - 2 * (num_ris - reg_number - 1) + 1, ri_tile.y}));
            ri_tile = static_cast<tile<Lyt>>(
                wire_west(layout, ri_tile, {reg_pos_x - 3 - 2 * (num_ris - reg_number - 1) + 1, ri_tile.y}));
            std::cout << ri_tile.x << std::endl;
        }
        else
        {
            for (auto index = ri_tile.x - 1; index > reg_pos_x - 3 - 2 * (num_ris - reg_number - 1); --index)
            {
                layout.assign_clock_number({index, ri_tile.y, 0}, pre_clock + 1);
                layout.assign_clock_number({index, ri_tile.y, 1}, pre_clock + 1);
                pre_clock = layout.get_clock_number({index, ri_tile.y});
            }
            ri_tile = static_cast<tile<Lyt>>(
                wire_west(layout, ri_tile, {reg_pos_x - 3 - 2 * (num_ris - reg_number - 1), ri_tile.y}));
        }
    }

    pre_clock = layout.get_clock_number({ri_tile});
    if (static_cast<tile<Lyt>>(node2pos[ctn.color_ntk.ro_at(reg_number)]).y == 0)
    {
        for (auto index = ri_tile.y - 1; index > static_cast<tile<Lyt>>(node2pos[ctn.color_ntk.ro_at(reg_number)]).y;
             --index)
        {
            layout.assign_clock_number({ri_tile.x, index, 0}, pre_clock + 1);
            pre_clock = layout.get_clock_number({ri_tile.x, index});
        }
        ri_tile = static_cast<tile<Lyt>>(wire_north(
            layout, ri_tile, {ri_tile.x, static_cast<tile<Lyt>>(node2pos[ctn.color_ntk.ro_at(reg_number)]).y}));
        ri_tile = static_cast<tile<Lyt>>(wire_north(
            layout, ri_tile, {ri_tile.x, static_cast<tile<Lyt>>(node2pos[ctn.color_ntk.ro_at(reg_number)]).y}));
    }
    else
    {
        for (auto index = ri_tile.y - 1;
             index > static_cast<tile<Lyt>>(node2pos[ctn.color_ntk.ro_at(reg_number)]).y - 1; --index)
        {
            layout.assign_clock_number({ri_tile.x, index, 0}, pre_clock + 1);
            pre_clock = layout.get_clock_number({ri_tile.x, index});
        }
        ri_tile = static_cast<tile<Lyt>>(wire_north(
            layout, ri_tile, {ri_tile.x, static_cast<tile<Lyt>>(node2pos[ctn.color_ntk.ro_at(reg_number)]).y - 1}));
    }
    wire_east(layout, ri_tile, {reg_pos_x, ri_tile.y});
}

template <typename Lyt, typename Ntk>
class orthogonal_sequential_network_impl
{
  public:
    orthogonal_sequential_network_impl(const Ntk& src, const orthogonal_physical_design_params& p,
                                       orthogonal_physical_design_stats& st) :
            ntk{mockturtle::fanout_view{
                fanout_substitution<mockturtle::names_view<mockturtle::sequential<technology_network>>>(src)}},
            ps{p},
            pst{st}
    {}

    Lyt run()
    {
        // measure run time
        mockturtle::stopwatch stop{pst.time_total};
        // compute a coloring
        const auto ctn = east_south_edge_coloring(ntk);

        mockturtle::node_map<mockturtle::signal<Lyt>, decltype(ctn.color_ntk)> node2pos{ctn.color_ntk};

        // instantiate the layout
        auto aspect_ratio = determine_sequential_layout_size<Lyt>(ctn);
        Lyt  layout{aspect_ratio, twoddwave_clocking<Lyt>(ps.number_of_clock_phases)};

        // reserve PI nodes without positions
        auto pi2node = reserve_input_nodes(layout, ctn.color_ntk);

        // reserve RO nodes without positions
        auto ro2node = reserve_register_output_nodes(layout, ctn.color_ntk);

        const uint32_t num_ris = ctn.color_ntk.num_registers();

        // first x-pos to use for gates is 1 because PIs take up the 0th column
        tile<Lyt> latest_pos{1 + 2 * num_ris, 0};

#if (PROGRESS_BARS)
        // initialize a progress bar
        mockturtle::progress_bar bar{ctn.color_ntk.size(), "[i] arranging layout: |{0}|"};
#endif

        ctn.color_ntk.foreach_node(
            [&](const auto& n, [[maybe_unused]] const auto i)
            {
                // do not place constants
                if (!ctn.color_ntk.is_constant(n))
                {
                    // if node is a PI, move it to its correct position
                    if (ctn.color_ntk.is_pi(n))
                    {
                        node2pos[n] = layout.move_node(pi2node[n], {0, latest_pos.y});

                        // resolve conflicting PIs
                        ctn.color_ntk.foreach_fanout(
                            n,
                            [&ctn, &n, &layout, &node2pos, &latest_pos](const auto& fon)
                            {
                                if (ctn.color_ntk.color(fon) == ctn.color_south)
                                {
                                    node2pos[n] = layout.create_buf(
                                        wire_east(layout, {0, latest_pos.y}, {latest_pos.x, latest_pos.y}),
                                        {latest_pos.x, latest_pos.y});
                                    ++latest_pos.x;
                                }
                                else if (ctn.color_ntk.color(fon) == ctn.color_east)
                                {
                                    node2pos[n] =
                                        layout.create_buf(wire_east(layout, {0, latest_pos.y}, latest_pos), latest_pos);
                                    ++latest_pos.x;
                                }

                                // PIs have only one fanout
                                return false;
                            });

                        ++latest_pos.y;
                    }
                    else if (ctn.color_ntk.is_ro(n))
                    {
                        node2pos[n] = layout.move_node(ro2node[n], {2 * num_ris, latest_pos.y});

                        // resolve conflicting PIs
                        ctn.color_ntk.foreach_fanout(
                            n,
                            [&ctn, &n, &layout, &node2pos, &latest_pos, &num_ris](const auto& fon)
                            {
                                if (ctn.color_ntk.color(fon) == ctn.color_south)
                                {
                                    node2pos[n] = layout.create_buf(
                                        wire_east(layout, {2 * num_ris, latest_pos.y}, latest_pos), latest_pos);
                                    ++latest_pos.x;
                                }

                                // PIs have only one fanout
                                return false;
                            });

                        ++latest_pos.y;
                    }
                    // if n has only one fanin
                    else if (const auto fc = fanins(ctn.color_ntk, n); fc.fanin_nodes.size() == 1)
                    {
                        const auto& pre = fc.fanin_nodes[0];

                        const auto pre_t = static_cast<tile<Lyt>>(node2pos[pre]);

                        // n is colored east
                        if (const auto clr = ctn.color_ntk.color(n); clr == ctn.color_east)
                        {
                            const tile<Lyt> t{latest_pos.x, pre_t.y};
                            node2pos[n] = connect_and_place(layout, t, ctn.color_ntk, n, pre_t);
                            ++latest_pos.x;
                        }
                        // n is colored south
                        else if (clr == ctn.color_south)
                        {
                            const tile<Lyt> t{pre_t.x, latest_pos.y};
                            node2pos[n] = connect_and_place(layout, t, ctn.color_ntk, n, pre_t);
                            ++latest_pos.y;
                        }
                        else
                        {
                            // single fanin nodes should not be colored null
                            assert(false);
                        }
                    }
                    else  // if node has two fanins (or three fanins with one of them being constant)
                    {
                        const auto &pre1 = fc.fanin_nodes[0], pre2 = fc.fanin_nodes[1];

                        auto pre1_t = static_cast<tile<Lyt>>(node2pos[pre1]),
                             pre2_t = static_cast<tile<Lyt>>(node2pos[pre2]);

                        tile<Lyt> t{};

                        // n is colored east
                        if (const auto clr = ctn.color_ntk.color(n); clr == ctn.color_east)
                        {
                            // make sure pre1_t is the northward tile and pre2_t is the westwards one
                            if (pre2_t.y < pre1_t.y)
                            {
                                std::swap(pre1_t, pre2_t);
                            }

                            // use larger y position of predecessors
                            t = {latest_pos.x, pre2_t.y};

                            // each 2-input gate has one incoming bent wire
                            pre1_t = static_cast<tile<Lyt>>(wire_east(layout, pre1_t, {t.x + 1, pre1_t.y}));

                            ++latest_pos.x;
                        }
                        // n is colored south
                        else if (clr == ctn.color_south)
                        {
                            // make sure pre1_t is the northward tile and pre2_t is the westwards one
                            if (pre2_t.x > pre1_t.x)
                            {
                                std::swap(pre1_t, pre2_t);
                            }

                            // use larger x position of predecessors
                            t = {pre1_t.x, latest_pos.y};

                            // each 2-input gate has one incoming bent wire
                            pre2_t = static_cast<tile<Lyt>>(wire_south(layout, pre2_t, {pre2_t.x, t.y + 1}));

                            ++latest_pos.y;
                        }
                        // n is colored null; corner case
                        else
                        {
                            // make sure pre1_t has an empty tile to its east and pre2_t to its south
                            if (!layout.is_empty_tile(layout.east(pre1_t)) ||
                                !layout.is_empty_tile(layout.south(pre2_t)))
                            {
                                std::swap(pre1_t, pre2_t);
                            }

                            t = latest_pos;

                            // both wires have one bent
                            pre1_t = static_cast<tile<Lyt>>(wire_east(layout, pre1_t, {t.x + 1, pre1_t.y}));
                            pre2_t = static_cast<tile<Lyt>>(wire_south(layout, pre2_t, {pre2_t.x, t.y + 1}));

                            ++latest_pos.x;
                            ++latest_pos.y;
                        }

                        node2pos[n] = connect_and_place(layout, t, ctn.color_ntk, n, pre1_t, pre2_t, fc.constant_fanin);
                    }

                    // create PO at applicable position
                    if (ctn.color_ntk.is_po(n))
                    {
                        const auto n_s = node2pos[n];

                        tile<Lyt> po_tile{};

                        bool south{false};

                        // determine PO orientation
                        if (is_eastern_po_orientation_available(ctn, n))
                        {
                            po_tile = layout.east(static_cast<tile<Lyt>>(n_s));
                            //++latest_pos.x;
                        }
                        else
                        {
                            po_tile = layout.south(static_cast<tile<Lyt>>(n_s));
                            ++latest_pos.y;
                            south = true;
                        }

                        // check if PO position is located at the border
                        if (layout.is_at_eastern_border(po_tile))
                        {
                            layout.create_po(n_s,
                                             ctn.color_ntk.has_output_name(po_counter) ?
                                                 ctn.color_ntk.get_output_name(po_counter++) :
                                                 fmt::format("po{}", po_counter++),
                                             po_tile);
                        }
                        // place PO at the border and connect it by wire segments
                        else
                        {
                            if (south)
                            {
                                const auto anker = layout.create_buf(n_s, po_tile);

                                po_tile = layout.eastern_border_of(po_tile);

                                layout.create_po(wire_east(layout, static_cast<tile<Lyt>>(anker), po_tile),
                                                 ctn.color_ntk.has_output_name(po_counter) ?
                                                     ctn.color_ntk.get_output_name(po_counter++) :
                                                     fmt::format("po{}", po_counter++),
                                                 po_tile);
                            }
                            else
                            {
                                tile<Lyt> anker = {po_tile.x - 1, po_tile.y};

                                po_tile = layout.eastern_border_of(po_tile);

                                layout.create_po(wire_east(layout, anker, po_tile),
                                                 ctn.color_ntk.has_output_name(po_counter) ?
                                                     ctn.color_ntk.get_output_name(po_counter++) :
                                                     fmt::format("po{}", po_counter++),
                                                 po_tile);
                            }
                        }
                    }
                    if (ctn.color_ntk.is_ri(n))
                    {
                        const auto n_s = node2pos[n];

                        tile<Lyt> ri_tile{};

                        const uint32_t reg_number = ctn.color_ntk.ro_index(ctn.color_ntk.ri_to_ro(n));

                        const auto global_syc_const = floor((ctn.color_ntk.num_cis()) / 4);

                        bool south{false};

                        // determine PO orientation
                        if (is_eastern_po_orientation_available(ctn, n) && !ctn.color_ntk.is_po(n))
                        {
                            ri_tile = layout.east(static_cast<tile<Lyt>>(n_s));
                            //++latest_pos.x;
                        }
                        else
                        {
                            ri_tile = layout.south(static_cast<tile<Lyt>>(n_s));
                            ++latest_pos.y;
                            south = true;
                        }

                        if (!south)
                        {
                            tile<Lyt> anker = {ri_tile.x - 1, ri_tile.y};

                            ri_tile   = layout.eastern_border_of(ri_tile);
                            ri_tile.x = aspect_ratio.x - ( num_ris - 1 ) * 4 + reg_number - num_ris - global_syc_const * 2;

                            layout.create_ri(wire_east(layout, anker, ri_tile), fmt::format("ri{}", ri_counter++),
                                             ri_tile);
                        }
                        else
                        {
                            const auto anker = layout.create_buf(n_s, ri_tile);

                            ri_tile   = layout.eastern_border_of(ri_tile);
                            ri_tile.x = aspect_ratio.x - ( num_ris - 1 ) * 4 + reg_number - num_ris - global_syc_const * 2;

                            layout.create_ri(wire_east(layout, static_cast<tile<Lyt>>(anker), ri_tile),
                                             fmt::format("ri{}", ri_counter++), ri_tile);
                        }
                        wire_registers(layout, ctn, ri_tile, num_ris, reg_number, aspect_ratio, node2pos, n);
                    }
                }

#if (PROGRESS_BARS)
                // update progress
                bar(i);
#endif
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
    mockturtle::topo_view<mockturtle::fanout_view<mockturtle::names_view<mockturtle::sequential<technology_network>>>>
        ntk;

    orthogonal_physical_design_params ps;
    orthogonal_physical_design_stats& pst;

    uint32_t po_counter{0};
    uint32_t ri_counter{0};
};

}  // namespace detail

template <typename Lyt, typename Ntk>
Lyt orthogonal_sequential_network(const Ntk& ntk, orthogonal_physical_design_params ps = {},
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

    orthogonal_physical_design_stats                     st{};
    detail::orthogonal_sequential_network_impl<Lyt, Ntk> p{ntk, ps, st};

    auto result = p.run();

    if (pst)
    {
        *pst = st;
    }

    return result;
}
}  // namespace fiction

#endif  // FICTION_ORTHOGONAL_SEQUENTIAL_NETWORK_HPP
