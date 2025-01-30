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
#include <array>
#include <cassert>
#include <cstdint>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#if (PROGRESS_BARS)
#include <mockturtle/utils/progress_bar.hpp>
#endif

namespace fiction
{

namespace detail
{

// Define a 3D array using std::array and encapsulate it in a function
std::array<std::array<std::array<std::array<std::pair<uint64_t, uint64_t>, 4>, 2>, 3>, 2>& get_buffer_lookup()
{
    static std::array<std::array<std::array<std::array<std::pair<uint64_t, uint64_t>, 4>, 2>, 3>, 2> array =
    {{ // Array
        {{ // Unconnected
            {{ // East
                {{
                        {0, 0}, {1, 0}, {1, 1}, {0, 1} // gap 0
                }},
                {{
                        {0, 0}, {0, 0}, {1, 0}, {0, 0} // gap 1
                }}
            }},
            {{ // South
                {{
                        {3, 0}, {2, 0}, {0, 0}, {0, 0} // only first two entries used
                }},
                {{
                        {0, 0}, {0, 0}, {0, 0}, {0, 0} // unused
                }}
            }},
            {{ // Free
                {{
                        {0, 0}, {1, 0}, {2, 0}, {3, 0} // gap 0
                }},
                {{
                        {0, 0}, {0, 0}, {1, 0}, {0, 0} // gap 1
                }}
            }}
        }},
        {{ // Connected
            {{ // East
                {{
                    {0, 0}, {0, 0}, {0, 1}, {0, 1} // gap 0
                }},
                {{
                    {0, 0}, {0, 0}, {0, 0}, {0, 0} // gap 1
                }}
            }},
            {{ // South
                {{
                    {3, 0}, {3, 0}, {0, 0}, {0, 0} // only first two entries used
                }},
                {{
                    {0, 0}, {0, 0}, {0, 0}, {0, 0} // unused
                }}
            }},
            {{ // Free
                {{
                    {0, 0}, {0, 0}, {3, 0}, {3, 0} // gap 0
                }},
                {{
                      {0, 0}, {0, 0}, {0, 0}, {0, 0} // gap 1
                }}
            }}
        }}
    }};
    return array;
}

// Define a 3D array using std::array and encapsulate it in a function
std::array<std::array<std::array<std::array<std::pair<uint64_t, uint64_t>, 4>, 2>, 3>, 3>& get_fanout_lookup()
{
    static std::array<std::array<std::array<std::array<std::pair<uint64_t, uint64_t>, 4>, 2>, 3>, 3> array =
        {{ // Array
            {{ // Type Fo 1+2
                {{ // East
                  {{
                      {1, 0}, {1, 1}, {1, 2}, {1, 1} // gap 0
                  }},
                  {{
                      {1, 0}, {1, 0}, {1, 1}, {1, 0} // gap 1
                  }}
                }},
                {{ // South
                  {{
                      {2, 0}, {2, 1}, {0, 0}, {0, 0} // only first two entries used
                  }},
                  {{
                      {0, 0}, {0, 0}, {0, 0}, {0, 0} // unused
                  }}
                }},
                {{ // Free
                  {{
                      {1, 0}, {2, 0}, {2, 1}, {2, 0} // gap 0
                  }},
                  {{
                      {1, 0}, {1, 0}, {2, 0}, {1, 0} // gap 1
                  }}
                }}
            }},
            {{ // Type F1
                {{ // East
                  {{
                      {0, 0}, {0, 1}, {0, 2}, {0, 1} // gap 0
                  }},
                  {{
                      {0, 0}, {0, 0}, {0, 1}, {0, 0} // gap 1
                  }}
                }},
                {{ // South
                  {{
                      {3, 0}, {3, 1}, {0, 0}, {0, 0} // only first two entries used
                  }},
                  {{
                      {0, 0}, {0, 0}, {0, 0}, {0, 0} // unused
                  }}
                }},
                {{ // Free
                  {{
                      {0, 0}, {3, 0}, {3, 1}, {3, 0} // gap 0
                  }},
                  {{
                      {0, 0}, {0, 0}, {3, 0}, {0, 0} // gap 1
                  }}
                }}
            }},
            {{ // Type F2
                {{ // East
                  {{
                      {1, 0}, {1, 0}, {1, 1}, {1, 1} // gap 0
                  }},
                  {{
                      {1, 0}, {1, 0}, {1, 0}, {1, 0} // gap 1
                  }}
                }},
                {{ // South
                  {{
                      {2, 0}, {2, 0}, {0, 0}, {0, 0} // only first two entries used
                  }},
                  {{
                      {0, 0}, {0, 0}, {0, 0}, {0, 0} // unused
                  }}
                }},
                {{ // Free
                  {{
                      {1, 0}, {1, 0}, {2, 0}, {2, 0} // gap 0
                  }},
                  {{
                  {1, 0}, {1, 0}, {1, 0}, {1, 0} // gap 1
                  }}
                }}
            }}
        }};
    return array;
}

template <typename Ntk>
uint64_t calculate_fanout_connection_type(const Ntk& ntk, mockturtle::node<Ntk> n)
{
    // order the POs
    auto fo = ntk.fanout(n);
    assert(fo.size() == 2);
    std::sort(fo.begin(), fo.end(), [&ntk](int a, int b) { return ntk.rank_position(a) < ntk.rank_position(b); });
    if (ntk.fanin_size(fo[0]) == 1 && ntk.fanin_size(fo[1]) == 1)
    {
        return 0;
    }
    if (ntk.fanin_size(fo[0]) == 1)
    {
        return 1;
    }
    if (ntk.fanin_size(fo[1]) == 1)
    {
        return 2;
    }
    // both fan-puts are connected with a neighbour
    return 3;
}

template <typename Ntk, typename Lyt>
uint64_t calculate_predecessor_gap(const Ntk& ntk, mockturtle::node_map<mockturtle::signal<Lyt>, Ntk>& node2pos,
                                   uint64_t lvl, mockturtle::node<Ntk> n)
{
    // return if in the PI level
    if (lvl == 0)
    {
        return 0;
    }

    // calculate the rank of the predecessor node
    auto fc = fanins(ntk, n);

    mockturtle::node<Ntk> nd = n;
    if (fc.fanin_nodes.size() == 2)
    {
        std::sort(fc.fanin_nodes.begin(), fc.fanin_nodes.end(),
                  [&ntk](int a, int b) { return ntk.rank_position(a) < ntk.rank_position(b); });
    }
    auto       pre = fc.fanin_nodes[0];
    const auto r   = ntk.rank_position(pre);

    // return if no neighbour
    if (r == 0)
    {
        return 0;
    }

    // calculate the level of the predecessor node
    const auto l = lvl - 1;

    // get the neighbour with lower rank of the predecessor
    const auto pre_neighbour = ntk.at_rank_position(l, r - 1);

    // calculate the gap size
    auto pre1_t = static_cast<tile<Lyt>>(node2pos[pre]);
    auto pre2_t = static_cast<tile<Lyt>>(node2pos[pre_neighbour]);

    assert(pre1_t.y > pre2_t.y);
    return pre1_t.y - pre2_t.y - 1;
}

template <typename Ntk>
uint64_t calculate_connection(const Ntk& ntk, mockturtle::node<Ntk> n)
{
    if (ntk.is_po(n))
    {
        return false;
    }
    auto fo = ntk.fanout(n);
    assert(fo.size() == 1);
    if (ntk.fanin_size(fo[0]) == 2)
    {
        auto fc  = fanins(ntk, fo[0]);
        auto pre = fc.fanin_nodes;
        assert(pre.size() == 2);
        std::sort(pre.begin(), pre.end(), [&ntk](int a, int b) { return ntk.rank_position(a) < ntk.rank_position(b); });
        if (pre[1] == n)
        {
            return 1;
        }
    }
    return 0;
}

template <typename Ntk>
uint64_t calculate_allowed_orientation(const Ntk& ntk, mockturtle::node<Ntk> n)
{
    auto fc = fanins(ntk, n);
    assert(fc.fanin_nodes.size() == 1);
    auto pre = fc.fanin_nodes[0];
    if (ntk.is_fanout(pre))
    {
        auto fo = ntk.fanout(pre);
        assert(fo.size() == 2);
        std::sort(fo.begin(), fo.end(), [&ntk](int a, int b) { return ntk.rank_position(a) < ntk.rank_position(b); });
        if (n == fo[0])  // east
        {
            return 0;
        }
        assert(n == fo[1]);
        return 1;
    }
    return 2;
}

template <typename Ntk, typename Lyt>
uint64_t calculate_max_new_lines(const Ntk& ntk,
                       mockturtle::node_map<mockturtle::signal<Lyt>, Ntk>& node2pos, uint32_t lvl)
{
    uint64_t max_nl = 0;
    ntk.foreach_node_in_rank(lvl,
                             [&ntk, &node2pos, &max_nl](const auto& n, const auto& i)
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
                                 }
                             });
    return max_nl;
}

template <typename Ntk, typename Lyt>
std::tuple<std::vector<uint64_t>, std::vector<uint64_t>>
compute_pr_variables(const Ntk& ntk, const Lyt& lyt, mockturtle::node_map<mockturtle::signal<Lyt>, Ntk> node2pos,
                     uint32_t lvl)
{
    std::vector<uint64_t> orientation(ntk.rank_width(lvl));
    std::vector<uint64_t> new_lines(ntk.rank_width(lvl));
    // get the lookup tables for the gate types
    const auto& buffer_lu = get_buffer_lookup();
    const auto& fanout_lu = get_fanout_lookup();

    ntk.foreach_node_in_rank(
        lvl,
        [&ntk, &node2pos, &lvl, &orientation, &new_lines, &buffer_lu, &fanout_lu](const auto& n, const auto& i)
        {
            // calculate the gap between the predecessors
            uint64_t gap = 0;  // calculate_predecessor_gap<Ntk, Lyt>(ntk, node2pos, lvl, n);
            if (n == 11)
            {
                gap = 1;
            }

            // needs gap and orientation as input
            if (ntk.fanin_size(n) == 2)  // complete
            {
                if (i != 0)
                {
                    if (orientation[i - 1] == 2 && gap == 0)
                    {
                        orientation[i] = 1;
                    }
                }
            }
            else if (ntk.fanin_size(n) == 1)
            {
                // allowed orientation Flag e = 0, s = 1, free = 2
                uint64_t allowed_orientation = calculate_allowed_orientation(ntk, n);

                // needs the type of connection (F1+2, F1, F2), allowed_orientation, gap,
                // orientation, new_line as input
                if (ntk.is_fanout(n))
                {
                    if (i != 0)
                    {
                        // calculate the type of connection F1+2 = 0, F1 = 1, F2 = 2;
                        uint64_t fanout_connection_type = calculate_fanout_connection_type(ntk, n);

                        const std::pair<uint64_t, uint64_t> pair =
                            fanout_lu[fanout_connection_type][allowed_orientation][gap][orientation[i - 1]];
                        orientation[i] = pair.first;
                        new_lines[i] = pair.second;
                    }
                }
                // needs the type of connection (connected, unconnected), allowed_orientation, gap,
                // orientation, new_line as input
                else
                {
                    if (i != 0)
                    {
                        // Connected Flag
                        uint64_t                            connected = calculate_connection(ntk, n);
                        const std::pair<uint64_t, uint64_t> pair =
                            buffer_lu[connected][allowed_orientation][gap][orientation[i - 1]];
                        orientation[i] = pair.first;
                        new_lines[i] = pair.second;
                    }
                }
            }
        });

    const auto ret = std::make_tuple(orientation, new_lines);
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

        tile<Lyt> place_t{0, 0};
        tile<Lyt> first_pos = {ntk.num_pis(), 0};

        for (uint32_t lvl = 0; lvl < ntk.depth() + 1; lvl++)
        {
            const auto variable_tuple = compute_pr_variables(ntk, layout, node2pos, lvl);
            const auto orientation    = std::get<0>(variable_tuple);
            const auto new_lines      = std::get<1>(variable_tuple);

            ntk.foreach_node_in_rank(
                lvl,
                [this, &layout, &pi2node, &node2pos, &orientation, &first_pos, &place_t](const auto& n,
                                                                                                          const auto& i)
                {
                    std::cout << "Node: " << n << " , Orientation: " << orientation[i] << std::endl;
                    if (!ntk.is_constant(n))
                    {
                        // if node is a PI, move it to its correct position
                        if (ntk.is_pi(n))
                        {
                            if (ntk.rank_position(n) == 0)
                            {
                                node2pos[n] = layout.move_node(pi2node[n], first_pos);
                                place_t     = first_pos;
                            }
                            else
                            {
                                place_t = {place_t.x - 1, place_t.y + 1};
                                // node2pos[n] = layout.move_node(pi2node[n], prec_pos);
                                if (place_t.x == 0)
                                {
                                    node2pos[n] = layout.move_node(pi2node[n], place_t);
                                }
                                else if (place_t.x < (first_pos.x / 2))
                                {
                                    node2pos[n] = layout.move_node(pi2node[n], {0, place_t.y});

                                    node2pos[n] =
                                        layout.create_buf(wire_east(layout, {0, place_t.y}, place_t), place_t);
                                }
                                else
                                {
                                    node2pos[n] = layout.move_node(pi2node[n], {place_t.x, 0});

                                    node2pos[n] =
                                        layout.create_buf(wire_south(layout, {place_t.x, 0}, place_t), place_t);
                                }
                            }
                        }
                        // if n has only one fanin
                        else if (const auto fc = fanins(ntk, n); fc.fanin_nodes.size() == 1)
                        {
                            const auto& pre   = fc.fanin_nodes[0];
                            auto        pre_t = static_cast<tile<Lyt>>(node2pos[pre]);

                            // horizontal (corresponding to colored east)
                            if (orientation[i] == 0 || orientation[i] == 1)
                            {
                                place_t.y  = pre_t.y;
                                place_t.x  = pre_t.x + 1;
                                node2pos[n] = connect_and_place(layout, place_t, ntk, n, pre_t);
                            }
                            else
                            {
                                assert(orientation[i] == 2 || orientation[i] == 3);
                                place_t.y  = pre_t.y + 1;
                                place_t.x  = pre_t.x;
                                node2pos[n] = connect_and_place(layout, place_t, ntk, n, pre_t);
                            }

                            if (ntk.rank_position(n) == 0)
                            {
                                first_pos   = place_t;
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

                            place_t = {pre1_t.x, pre2_t.y};

                            node2pos[n] = connect_and_place(layout, place_t, ntk, n, pre1_t, pre2_t, fc.constant_fanin);

                            if (ntk.rank_position(n) == 0)
                            {
                                first_pos   = place_t;
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
                            if (po_tile.y == place_t.y)
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

        layout.resize({first_pos.x + 1, place_t.y + add_line, 0});

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
