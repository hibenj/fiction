//
// Created by benjamin on 7/29/24.
//

#ifndef FICTION_ORTHOGONAL_PLANAR_HPP
#define FICTION_ORTHOGONAL_PLANAR_HPP

#include "fiction/algorithms/network_transformation/fanout_substitution.hpp"
#include "fiction/algorithms/properties/check_planarity.hpp"
#include "fiction/io/print_layout.hpp"
#include "fiction/layouts/clocking_scheme.hpp"
#include "fiction/networks/views/edge_color_view.hpp"
#include "fiction/traits.hpp"
#include "fiction/utils/name_utils.hpp"
#include "fiction/utils/network_utils.hpp"
#include "fiction/utils/placement_utils.hpp"

#include <fmt/format.h>
#include <mockturtle/traits.hpp>
#include <mockturtle/utils/node_map.hpp>
#include <mockturtle/utils/stopwatch.hpp>
#include <mockturtle/views/fanout_view.hpp>
#include <mockturtle/views/rank_view.hpp>
#include <mockturtle/views/topo_view.hpp>

#include <algorithm>
#include <optional>
#include <set>
#include <vector>

#if (PROGRESS_BARS)
#include <mockturtle/utils/progress_bar.hpp>
#endif

namespace fiction
{

/**
 * Parameters for the orthogonal physical design algorithm.
 */
struct orthogonal_physical_design_params
{
    /**
     * Number of clock phases to use. 3 and 4 are supported.
     */
    num_clks number_of_clock_phases = num_clks::FOUR;
};

struct orthogonal_physical_design_stats
{
    mockturtle::stopwatch<>::duration time_total{0};

    uint64_t x_size{0ull}, y_size{0ull};
    uint64_t num_gates{0ull}, num_wires{0ull};

    void report(std::ostream& out = std::cout) const
    {
        out << fmt::format("[i] total time  = {:.2f} secs\n", mockturtle::to_seconds(time_total));
        out << fmt::format("[i] layout size = {} × {}\n", x_size, y_size);
        out << fmt::format("[i] num. gates  = {}\n", num_gates);
        out << fmt::format("[i] num. wires  = {}\n", num_wires);
    }
};

/**
 * 1. Find widest level
 * X. [Analyze network structure in order to llo network to get smaller]
 * 2. From the widest level place its FIs and FOs on the layout
 *
 *
 *
 */

namespace detail
{

/**
 * Determine siblings of the given node. A sibling is a node that shares the same fan-in with n.
 * @param n Node to consider.
 * @return Siblings of n.
 */
template <typename Ntk>
std::vector<mockturtle::node<Ntk>> siblings(const Ntk& ntk, const mockturtle::node<Ntk> n) noexcept
{
    std::vector<mockturtle::node<Ntk>> sibs{};
    ntk.foreach_fanin(n,
                      [&ntk, &sibs, &n](const auto& fi)
                      {
                          // skip constants
                          if (const auto fin = ntk.get_node(fi); !ntk.is_constant(fin))
                          {
                              ntk.foreach_fanout(fin,
                                                 [&ntk, &sibs, &n](const auto& fon)
                                                 {
                                                     // do not consider constants or n itself
                                                     if (!ntk.is_constant(fon) && (fon != n))
                                                     {
                                                         sibs.push_back(fon);
                                                     }
                                                 });
                          }
                      });

    return sibs;
}

template <typename Ntk>
struct coloring_container
{
    explicit coloring_container(const Ntk& ntk) :
            color_ntk{ntk},
            color_east{color_ntk.new_color()},
            color_south{color_ntk.new_color()}
    {}

    out_of_place_edge_color_view<Ntk> color_ntk;

    const uint32_t color_null = 0ul, color_east, color_south;

    [[nodiscard]] uint32_t opposite_color(const uint32_t c) const noexcept
    {
        return c == color_east ? color_south : color_east;
    }
};

template <typename Ntk>
void recursively_paint_edges(const coloring_container<Ntk>&                             ctn,
                             const mockturtle::edge<out_of_place_edge_color_view<Ntk>>& e, const uint32_t c) noexcept
{
    // exit condition: edge is already painted
    if (ctn.color_ntk.edge_color(e) != ctn.color_null)
    {
        return;
    }

    // paint edge with given color
    ctn.color_ntk.paint_edge(e, c);

    // paint children edges
    foreach_outgoing_edge(ctn.color_ntk, e.source,
                          [&ctn, &e, &c](const auto& oe)
                          {
                              if (oe != e)
                              {
                                  recursively_paint_edges(ctn, oe, ctn.opposite_color(c));
                              }
                          });

    // paint spouse edges
    foreach_incoming_edge(ctn.color_ntk, e.target,
                          [&ctn, &e, &c](const auto& ie)
                          {
                              if (ie != e)
                              {
                                  recursively_paint_edges(ctn, ie, c);
                              }
                          });
}

template <typename Ntk>
coloring_container<Ntk> east_south_edge_coloring(const Ntk& ntk) noexcept
{
    coloring_container<Ntk> ctn{ntk};
    mockturtle::topo_view   rtv{ntk};

#if (PROGRESS_BARS)
    // initialize a progress bar
    mockturtle::progress_bar bar{static_cast<uint32_t>(ctn.color_ntk.num_gates()),
                                 "[i] determining relative positions: |{0}|"};
#endif

    rtv.foreach_gate(
        [&](const auto& n, [[maybe_unused]] const auto i)
        {
            const auto finc = fanin_edges(ctn.color_ntk, n);

            // if any incoming edge is colored east, color them all east, and south otherwise
            const auto color = std::any_of(finc.fanin_edges.cbegin(), finc.fanin_edges.cend(), [&ctn](const auto& fe)
                                           { return ctn.color_ntk.edge_color(fe) == ctn.color_east; }) ?
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

    return ctn;
}

template <typename Ntk>
bool is_east_south_colored(const Ntk& ntk) noexcept
{
    bool is_properly_colored = true;
    ntk.foreach_node(
        [&ntk, &is_properly_colored](const auto& n)
        {
            if (!ntk.is_constant(n))
            {
                uint32_t clr = 0ul;
                ntk.foreach_fanout(n,
                                   [&ntk, &is_properly_colored, &clr](const auto& fon, const auto i)
                                   {
                                       // store color of first fanout
                                       if (i == 0)
                                       {
                                           clr = ntk.color(fon);
                                       }
                                       // compare color of first fanout against all others
                                       else if (clr == ntk.color(fon))
                                       {
                                           // since we are working with 3-graphs, this can only be executed if i == 1
                                           is_properly_colored = false;
                                       }

                                       return is_properly_colored;
                                   });
            }
            return is_properly_colored;
        });

    return is_properly_colored;
}

template <typename Ntk>
uint32_t is_eastern_po_orientation_available(const coloring_container<Ntk>& ctn,
                                             const mockturtle::node<Ntk>&   n) noexcept
{
    bool eastern_side_available = true;

    // PO nodes can have a maximum of one other fanout
    ctn.color_ntk.foreach_fanout(n,
                                 [&ctn, &eastern_side_available](const auto& fon)
                                 {
                                     // if that fanout is colored east, no PO can be placed there
                                     if (ctn.color_ntk.color(fon) == ctn.color_east)
                                     {
                                         eastern_side_available = false;
                                     }
                                     return false;
                                 });

    return eastern_side_available;
}

template <typename Lyt, typename Ntk>
aspect_ratio<Lyt> determine_layout_size(const coloring_container<Ntk>& ctn,
                                        const uint32_t                 num_multi_output_nodes) noexcept
{
#if (PROGRESS_BARS)
    // initialize a progress bar
    mockturtle::progress_bar bar{static_cast<uint32_t>(ctn.color_ntk.size()), "[i] determining layout size: |{0}|"};
#endif

    uint64_t x = 0ull, y = ctn.color_ntk.num_pis() - 1;
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
                    if (is_eastern_po_orientation_available(ctn, n))
                    {
                        ++x;
                    }
                    else
                    {
                        ++y;
                    }
                }
            }

#if (PROGRESS_BARS)
            // update progress
            bar(i);
#endif
        });

    // for multi-output nodes, add another row
    y += num_multi_output_nodes;

    return {x, y, 1};
}

template <typename Lyt>
mockturtle::signal<Lyt> wire_east(Lyt& lyt, const tile<Lyt>& src, const tile<Lyt>& dest)
{
    auto a = static_cast<mockturtle::signal<Lyt>>(src);

    for (auto x = src.x + 1; x < dest.x; ++x)
    {
        auto t = tile<Lyt>{x, src.y, 0};
        if (!lyt.is_empty_tile(t))  // crossing case
        {
            t = lyt.above(t);
        }

        a = lyt.create_buf(a, t);
    }

    return a;
}

template <typename Lyt>
mockturtle::signal<Lyt> wire_south(Lyt& lyt, const tile<Lyt>& src, const tile<Lyt>& dest)
{
    auto a = static_cast<mockturtle::signal<Lyt>>(src);

    for (auto y = src.y + 1; y < dest.y; ++y)
    {
        auto t = tile<Lyt>{src.x, y, 0};
        if (!lyt.is_empty_tile(t))  // crossing case
        {
            t = lyt.above(t);
        }

        a = lyt.create_buf(a, t);
    }

    return a;
}

template <typename Lyt, typename Ntk>
mockturtle::signal<Lyt> connect_and_place(Lyt& lyt, const tile<Lyt>& t, const Ntk& ntk, const mockturtle::node<Ntk>& n,
                                          tile<Lyt> pre1_t, tile<Lyt> pre2_t,
                                          const std::optional<bool>& c = std::nullopt)
{
    // make sure pre1_t is the northwards tile and pre2_t is the westwards one
    if (pre2_t < pre1_t)
    {
        std::swap(pre1_t, pre2_t);
    }

    return place(lyt, t, ntk, n, wire_south(lyt, pre1_t, t), wire_east(lyt, pre2_t, t), c);
}

template <typename Lyt, typename Ntk>
mockturtle::signal<Lyt> connect_and_place(Lyt& lyt, const tile<Lyt>& t, const Ntk& ntk, const mockturtle::node<Ntk>& n,
                                          const tile<Lyt>& pre_t)
{
    if (lyt.is_westwards_of(t, pre_t))
    {
        return place(lyt, t, ntk, n, wire_east(lyt, pre_t, t));
    }
    if (lyt.is_northwards_of(t, pre_t))
    {
        return place(lyt, t, ntk, n, wire_south(lyt, pre_t, t));
    }

    assert(false);  // gates cannot be placed elsewhere
    return {};      // fix -Wreturn-type warning
}

/**
 * Places the primary outputs (POs) in the layout.
 *
 * This function positions the POs within the provided layout. If a PO is a multi-output node (a fanout with two POs
 * connected to it), the second PO is automatically placed and connected below the first output.
 *
 * The positioning of multi-output nodes will follow this structure:
 *
 * F→=→...→O
 * ↓
 * =→=→...→O
 *
 * @tparam Lyt Desired gate-level layout type.
 * @tparam Ntk Network type that acts as specification.
 * @param layout Current gate-level layout.
 * @param ctn Contains the colored network.
 * @param po_counter Counter for POs.
 * @param node2pos Mapping from network nodes to layout signals, i.e., a pointer to their position in the layout. The
 * map is used to fetch location of the fanins. The `mockturtle::node_map` is not updated by this function.
 */
template <typename Ntk, typename Lyt>
void place_outputs(Lyt& layout, const coloring_container<Ntk>& ctn, uint32_t po_counter,
                   const mockturtle::node_map<mockturtle::signal<Lyt>, decltype(ctn.color_ntk)>& node2pos)
{
    std::vector<mockturtle::node<Ntk>> output_nodes{};

    ctn.color_ntk.foreach_po(
        [&po_counter, &output_nodes, &node2pos, &ctn, &layout](const auto& po)
        {
            if (!ctn.color_ntk.is_constant(po))
            {
                const auto n_s     = node2pos[po];
                auto       po_tile = static_cast<tile<Lyt>>(n_s);

                const auto multi_output_node =
                    std::find(output_nodes.cbegin(), output_nodes.cend(), po) != output_nodes.cend();

                // determine PO orientation
                if (!is_eastern_po_orientation_available(ctn, po) || multi_output_node)
                {
                    po_tile = static_cast<tile<Lyt>>(wire_south(layout, po_tile, {po_tile.x, po_tile.y + 2}));
                }

                // check if PO position is located at the border
                if (layout.is_at_eastern_border({po_tile.x + 1, po_tile.y}) && !multi_output_node)
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
                }

                output_nodes.push_back(po);
            }
        });
}

template <typename Ntk>
void fill_gap_array_zeros(Ntk& ntk, std::vector<std::vector<int>>& gap_array)
{
    for (int i = 0; i < ntk.depth() + 1; i++)
    {
        gap_array[i] = std::vector<int>(ntk.rank_width(i), 0);
    }
}

template <typename Ntk>
void fill_gap_array_zeros(Ntk& ntk, std::vector<std::vector<int>>& gap_array, int start, int end)
{
    for (int i = start; i < end + 1; i++)
    {
        gap_array[i] = std::vector<int>(ntk.rank_width(i), 0);
    }
}

template <typename Ntk>
uint32_t propagate_forward(Ntk& ntk, std::vector<std::vector<int>>& fw_gap_array,
                           std::vector<std::vector<int>>& bw_gap_array, uint32_t starting_rank, bool& is_negative,
                           std::vector<int>& orientations)
{
    debug::write_dot_network(ntk);

    typename Ntk::node last_visited_node;
    // orientation: 1 = vertical, 2 = horizontal
    int      orientation            = 0;
    uint32_t starting_rank_backward = 0;
    int      fo_gap                 = 0;

    for (uint32_t r = starting_rank; r < ntk.depth(); r++)
    {
        // identify_structures(ntk);
        ntk.foreach_node_in_rank(
            r,
            [&ntk, &fw_gap_array, &bw_gap_array, &last_visited_node, &r, &orientation, &fo_gap, &orientations](const auto& n)
            {
                const auto fo = ntk.fanout(n);
                // fi of buffer/inv
                if (ntk.fanin_size(fo[0]) == 1 && fo.size() == 1)
                {
                    if (ntk.rank_position(n) != ntk.rank_width(r) - 1)
                    {
                        // propagate gap: array[r + 1, rank(fo[0])] += array[r, rank(n)]
                        fw_gap_array[r + 1][ntk.rank_position(fo[0])] += fw_gap_array[r][ntk.rank_position(n)];
                    }
                    last_visited_node = fo[0];
                    // orientation stays the same
                }
                // fanout
                else if (ntk.is_fanout(n) && fo.size() == 2)
                {
                    int max_value_index = std::max_element(fo.begin(), fo.end(), [&](const auto& a, const auto& b)
                                                           { return ntk.rank_position(a) < ntk.rank_position(b); }) -
                                          fo.begin();

                    // chain starts here
                    if (ntk.rank_position(n) != 0 && last_visited_node != fo[1 - max_value_index])
                    {
                        if (orientation == 1)
                        {
                            fw_gap_array[r + 1][ntk.rank_position(fo[1 - max_value_index]) - 1] -= fo_gap;
                            // array[r + 1, rank(fo[1 - max_value_index]) - 1] -= 1
                            fw_gap_array[r + 1][ntk.rank_position(fo[1 - max_value_index]) - 1] -= 1;
                        }
                    }

                    // chain ends here
                    if (ntk.fanin_size(fo[max_value_index]) == 1)
                    {
                        if (ntk.rank_position(n) != ntk.rank_width(r) - 1)
                        {
                            // propagate gap: array[r + 1, rank(fo[max_value_index])] += array[r,
                            // rank(n)]
                            fw_gap_array[r + 1][ntk.rank_position(fo[max_value_index])] +=
                                fw_gap_array[r][ntk.rank_position(n)];
                            orientation = 1;
                        }
                    }
                    else  // chain extends
                    {
                        // if array[r, rank(n)] > 0, then x new lines and propagate to left
                        // rank(fo[0]) - 1
                        if (ntk.rank_position(n) != 0 && fw_gap_array[r][ntk.rank_position(n)] > 0)
                        {
                            // the last entry holds the new line entry
                            fw_gap_array[r + 1].back() = fw_gap_array[r][ntk.rank_position(n)];

                            // shift gap to the left
                            fw_gap_array[r + 1][ntk.rank_position(fo[0]) - 1] += fw_gap_array[r][ntk.rank_position(n)];
                        }
                    }

                    // check if between the fanouts there is a gap (this gap comes from backward propagation)
                    if (fw_gap_array[r + 1][ntk.rank_position(fo[1 - max_value_index])] > 0)
                    {
                        std::cout << "gap between fos\n";
                    }

                    if (bw_gap_array[r + 1][ntk.rank_position(fo[1 - max_value_index])] >
                        fw_gap_array[r + 1][ntk.rank_position(fo[1 - max_value_index])])
                    {
                        fw_gap_array[r + 1][ntk.rank_position(fo[1 - max_value_index])] =
                            bw_gap_array[r + 1][ntk.rank_position(fo[1 - max_value_index])];
                    }
                    fo_gap = fw_gap_array[r + 1][ntk.rank_position(fo[1 - max_value_index])];

                    last_visited_node = fo[max_value_index];

                    if (orientations[r+1] == 0)
                    {
                        orientations[r+1] = 2;
                    }
                }
                // fi of 2-input node
                else
                {
                    assert(ntk.fanin_size(fo[0]) == 2 && fo.size() == 1);

                    // chain starts here
                    if (ntk.rank_position(n) != 0 && last_visited_node != fo[0])
                    {
                        // if array[r, rank(n)] > 0, then x new lines and propagate to left
                        // rank(fo[0]) - 1
                        if (fw_gap_array[r][ntk.rank_position(n)] > 0)
                        {
                            // the last entry holds the new line entry
                            fw_gap_array[r + 1].back() = fw_gap_array[r][ntk.rank_position(n)];

                            // shift gap to the left
                            if (orientation == 2)
                            {
                                fw_gap_array[r + 1][ntk.rank_position(fo[0]) - 1] +=
                                    fw_gap_array[r][ntk.rank_position(n)];
                            }
                        }

                        if (orientation == 2)
                        {
                            // array[r + 1, rank(fo[0]-1)] += 1
                            fw_gap_array[r + 1][ntk.rank_position(fo[0]) - 1] += 1;
                        }
                        // if(orientation == 1) no gap
                    }
                    else if (last_visited_node == fo[0] &&
                             ntk.rank_position(n) != ntk.rank_width(r) - 1)  // chain ends here
                    {
                        // propagate gap: array[r + 1, rank(fo[0])] += array[r, rank(n)]
                        fw_gap_array[r + 1][ntk.rank_position(fo[0])] += fw_gap_array[r][ntk.rank_position(n)];
                        orientation = 2;
                    }

                    last_visited_node = fo[0];

                    if (orientations[r+1] == 0)
                    {
                        orientations[r+1] = 1;
                    }
                }
            });
        // compare for new lines
        fw_gap_array[r + 1].back() = std::max(fw_gap_array[r + 1].back(), bw_gap_array[r + 1].back());
        // if there is a negative gap, go to backward propagation
        is_negative = false;
        for (std::size_t i = 0; i < fw_gap_array[r].size() - 1; i++)
        {
            if (fw_gap_array[r + 1][i] == -1)
            {
                is_negative = true;
                break;
            }
        }

        starting_rank_backward = r;

        if (is_negative)
        {
            break;
        }
    }
    return starting_rank_backward;
}

template <typename Ntk>
uint32_t propagate_backward(Ntk& ntk, std::vector<std::vector<int>>& fw_gap_array,
                            std::vector<std::vector<int>>& bw_gap_array, uint32_t starting_rank)
{
    debug::write_dot_network(ntk);

    typename Ntk::node last_visited_node;
    // orientation: 1 = vertical, 2 = horizontal
    int      orientation           = 0;
    uint32_t starting_rank_forward = 0;

    assert(starting_rank < ntk.depth());

    for (uint32_t r = starting_rank; r <= ntk.depth(); r--)
    {
        ntk.foreach_node_in_rank(
            r,
            [&ntk, &bw_gap_array, &last_visited_node, &r, &orientation](const auto& n)
            {
                const auto fo = ntk.fanout(n);
                // fi of buffer/inv
                if (ntk.fanin_size(fo[0]) == 1 && fo.size() == 1)
                {
                    if (ntk.rank_position(n) != ntk.rank_width(r) - 1)
                    {
                        // propagate gap: array[r + 1, rank(fo[0])] += array[r, rank(n)]
                        bw_gap_array[r][ntk.rank_position(n)] += bw_gap_array[r + 1][ntk.rank_position(fo[0])];
                    }
                    last_visited_node = fo[0];
                    // orientation stays the same
                }
                // fanout
                else if (ntk.is_fanout(n) && fo.size() == 2)
                {
                    int max_value_index = std::max_element(fo.begin(), fo.end(), [&](const auto& a, const auto& b)
                                                           { return ntk.rank_position(a) < ntk.rank_position(b); }) -
                                          fo.begin();

                    // check 2 FOs behind each other: between them add a gap with 1
                    if (ntk.rank_position(n) != 0 && last_visited_node != fo[1 - max_value_index])
                    {
                        if (orientation == 1)
                        {
                            bw_gap_array[r][ntk.rank_position(n) - 1] += 1;
                        }
                    }

                    // chain ends: propagate the gap at the end of the chain
                    if (ntk.fanin_size(fo[max_value_index]) == 1 && ntk.rank_position(n) != ntk.rank_width(r) - 1)
                    {
                        bw_gap_array[r][ntk.rank_position(n)] +=
                            bw_gap_array[r + 1][ntk.rank_position(fo[max_value_index])];
                        orientation = 1;
                    }

                    // if there is a gap between the two fanout nodes: propagate to the preceding level (and right)
                    if (ntk.rank_position(n) != ntk.rank_width(r) - 1 &&
                        bw_gap_array[r + 1][ntk.rank_position(fo[1 - max_value_index])] > 0)
                    {
                        // the last entry holds the new line entry
                        bw_gap_array[r + 1].back() = bw_gap_array[r + 1][ntk.rank_position(fo[1 - max_value_index])];

                        // shift gap to the right
                        if (ntk.is_fanout(ntk.at_rank_position(r, ntk.rank_position(n) + 1)))
                        {
                            bw_gap_array[r][ntk.rank_position(n)] +=
                                bw_gap_array[r + 1][ntk.rank_position(fo[1 - max_value_index])];
                        }
                    }

                    last_visited_node = fo[max_value_index];
                }
                // fi of 2-input node
                else
                {
                    assert(ntk.fanin_size(fo[0]) == 2 && fo.size() == 1);

                    // chain starts here
                    if (ntk.rank_position(n) != 0 && last_visited_node != fo[0])
                    {
                        if (orientation == 2)
                        {
                            // array[r + 1, rank(fo[0]-1)] += 1
                            bw_gap_array[r][ntk.rank_position(n) - 1] -= 1;
                        }
                        // if(orientation == 1) no gap
                    }
                    else if (last_visited_node == fo[0] &&
                             ntk.rank_position(n) != ntk.rank_width(r) - 1)  // chain ends here
                    {
                        bw_gap_array[r][ntk.rank_position(n)] += bw_gap_array[r + 1][ntk.rank_position(fo[0])];
                        orientation = 2;
                    }

                    last_visited_node = fo[0];
                }
            });

        // terminate if there is no gap in the backward propagation that is bigger than a value of the matrix in the
        // forward propagation
        bool is_larger = false;
        for (std::size_t i = 0; i < bw_gap_array[r].size() - 1; i++)
        {
            if (bw_gap_array[r][i] > fw_gap_array[r][i])
            {
                is_larger = true;
                break;
            }
        }

        starting_rank_forward = r;

        if (!is_larger)
        {
            break;
        }

        if (r == 0)  // to stop at 0
        {
            break;
        }
    }
    return starting_rank_forward;
}

template <typename Lyt, typename Ntk>
class orthogonal_planar_impl
{
  public:
    orthogonal_planar_impl(const Ntk& src, const orthogonal_physical_design_params& p,
                           orthogonal_physical_design_stats& st) :
            ntk{extended_rank_view{
                mockturtle::fanout_view{fanout_substitution<mockturtle::names_view<technology_network>>(src)}}},
            ps{p},
            pst{st}
    {}

    Lyt run()
    {
        using node = typename Ntk::node;

        uint32_t starting_rank_forward  = 0;
        uint32_t starting_rank_backward = 0;

        std::vector<std::vector<int>> forward_gap_array(ntk.depth() + 1);
        fill_gap_array_zeros(ntk, forward_gap_array);

        std::vector<std::vector<int>> backward_gap_array(ntk.depth() + 1);
        fill_gap_array_zeros(ntk, backward_gap_array);

        // propagate forward
        // if a gap is negative: return this level as starting_rank_backward
        // set every gap in this line to 0 and propagate backwards
        // stop backward propagation if there is no backwards gap in the level, that is bigger than the corresponding:
        // return this level again as starting_rank_forward
        // start again with forward propagation

        bool             is_negative = false;
        std::vector<int> orientations(ntk.depth(), 0);

        starting_rank_backward = propagate_forward(ntk, forward_gap_array, backward_gap_array, starting_rank_forward,
                                                   is_negative, orientations);
        // starting_rank_foreward = propagate_backward(ntk, forward_gap_array, backward_gap_array,
        // starting_rank_backward);

        while (is_negative)
        {
            starting_rank_forward =
                propagate_backward(ntk, forward_gap_array, backward_gap_array, starting_rank_backward);
            fill_gap_array_zeros(ntk, forward_gap_array, starting_rank_forward + 1, starting_rank_backward + 1);
            // if starting rank = 0, dann muss man die PI gaps mit bw_gap_array setzen
            if (starting_rank_forward == 0)
            {
                for (std::size_t i = 0; i < backward_gap_array[0].size(); i++)
                {
                    if (backward_gap_array[0][i] > 0)
                    {
                        forward_gap_array[0][i] = backward_gap_array[0][i];
                    }
                }
            }
            starting_rank_backward = propagate_forward(ntk, forward_gap_array, backward_gap_array,
                                                       starting_rank_forward, is_negative, orientations);
            // is_negative = false;

            // fill_gap_array_zeros(ntk, backward_gap_array);
        }

        for (int i = 0; i < forward_gap_array.size(); i++)
        {
            for (int j = 0; j < forward_gap_array[i].size(); j++)
            {
                std::cout << forward_gap_array[i][j] << ',';
            }
            std::cout << "\n";
        }

        for (int i = 0; i < backward_gap_array.size(); i++)
        {
            for (int j = 0; j < backward_gap_array[i].size(); j++)
            {
                std::cout << backward_gap_array[i][j] << ',';
            }
            std::cout << "\n";
        }

        // measure run time
        mockturtle::stopwatch stop{pst.time_total};
        // compute a coloring
        const auto ctn = east_south_edge_coloring(ntk);

        ntk.width();

        mockturtle::node_map<mockturtle::signal<Lyt>, decltype(ctn.color_ntk)> node2pos{ctn.color_ntk};

        // find multi-output nodes
        std::vector<mockturtle::node<decltype(ntk)>> output_nodes{};
        std::vector<mockturtle::node<decltype(ntk)>> multi_output_nodes{};
        uint32_t                                     num_multi_output_nodes{0};

        ctn.color_ntk.foreach_po(
            [&](const auto& po)
            {
                if (std::find(output_nodes.cbegin(), output_nodes.cend(), po) != output_nodes.cend())
                {
                    multi_output_nodes.push_back(po);
                    ++num_multi_output_nodes;
                }

                output_nodes.push_back(po);
            });

        aspect_ratio<Lyt> size_ = {15, 15};
        // instantiate the layout
        Lyt layout{size_,
                   twoddwave_clocking<Lyt>(ps.number_of_clock_phases)};

        // reserve PI nodes without positions
        auto pi2node = reserve_input_nodes(layout, ctn.color_ntk);

        // first x-pos to use for gates is 1 because PIs take up the 0th column
        tile<Lyt> latest_pos{1, 0};

#if (PROGRESS_BARS)
        // initialize a progress bar
        mockturtle::progress_bar bar{ctn.color_ntk.size(), "[i] arranging layout: |{0}|"};
#endif

        const auto first_pi_node = [this](const auto& r, const auto& gaps)
        {
            int level_size = 0;
            if (r == 0)
            {
                level_size = std::accumulate(gaps.begin(), gaps.end() - 1, 0);
            }
            level_size += ntk.num_pis();

            return level_size - 1;
        };

        const auto get_orientation = [this](const auto& lvl)
        {
            // orientation: 1 = vertical, 2 = horizontal
            int orientation = 2;
            ntk.foreach_node_in_rank(lvl,
                                     [this, &orientation, &lvl](const auto& n)
                                     {
                                         const auto fo = ntk.fanout(n);
                                         if (fo.size() == 2)
                                         {
                                             if (ntk.rank_position(n) != 0)
                                             {
                                                 orientation = 2;
                                                 return;
                                             }
                                             else
                                             {
                                                 orientation = 1;
                                                 return;
                                             }
                                         }
                                         else if (ntk.fanin_size(n) == 2)
                                         {
                                             const auto rk = ntk.rank_position(n);
                                             if (rk != 0)
                                             {
                                                 const auto fc_pre =
                                                     fanins(ntk, ntk.at_rank_position(lvl, ntk.rank_position(n)) - 1);
                                                 const auto fc = fanins(ntk, n);
                                                 assert(fc_pre.fanin_nodes.size() == 1);
                                                 assert(fc.fanin_nodes.size() == 2);
                                                 if ((fc.fanin_nodes[0] == fc_pre.fanin_nodes[0]) ||
                                                     (fc.fanin_nodes[1] == fc_pre.fanin_nodes[0]))
                                                 {
                                                     orientation = 2;
                                                     return;
                                                 }
                                             }
                                             orientation = 1;
                                             return;
                                         }
                                     });
            return orientation;
        };

        tile<Lyt> prec_pos{0, 0};


        for (uint32_t lvl = 0; lvl < ntk.depth() + 1; lvl++)
        {
            auto        level_gaps  = forward_gap_array[lvl];
            auto        pi_sz       = first_pi_node(lvl, level_gaps);
            tile<Lyt>   first_pos   = {pi_sz, 0};
            std::size_t r           = 0;
            auto        orientation = orientations[lvl];
            ntk.foreach_node_in_rank(
                lvl,
                [this, &r, &level_gaps, &forward_gap_array, &first_pos, &prec_pos, &node2pos, &pi2node, &layout,
                 &orientation](const auto& n)
                {
                    if (!ntk.is_constant(n))
                    {
                        std::cout << n << " has rank 0 \n";
                        // if node is a PI, move it to its correct position
                        if (ntk.is_pi(n))
                        {
                            if (ntk.rank_position(n) == 0)
                            {
                                node2pos[n] = layout.move_node(pi2node[n], first_pos);
                                prec_pos    = first_pos;
                                std::cout << first_pos << "PI\n";
                            }
                            else
                            {
                                int gap     = level_gaps[r];
                                prec_pos    = {prec_pos.x - gap - 1, prec_pos.y + gap + 1};
                                node2pos[n] = layout.move_node(pi2node[n], prec_pos);
                            }
                        }
                        // if n has only one fanin
                        else if (const auto fc = fanins(ntk, n); fc.fanin_nodes.size() == 1)
                        {
                            const auto& pre       = fc.fanin_nodes[0];
                            const auto  pre_t     = static_cast<tile<Lyt>>(node2pos[pre]);
                            int         new_lines = level_gaps.back();
                            // horizontal (corresponding to colored east)
                            if (ntk.rank_position(n) == 0)
                            {
                                if (orientation == 1)
                                {
                                    first_pos   = {first_pos.x, first_pos.y + new_lines + 1};
                                }
                                else
                                {
                                    first_pos   = {first_pos.x + new_lines + 1, first_pos.y};
                                }

                                prec_pos    = first_pos;
                                node2pos[n] = connect_and_place(layout, first_pos, ntk, n, pre_t);
                            }
                            else
                            {
                                int gap     = level_gaps[r];
                                prec_pos    = {prec_pos.x - gap - 1, prec_pos.y + gap + 1};
                                node2pos[n] = connect_and_place(layout, prec_pos, ntk, n, pre_t);
                            }

                            std::cout << "Nodes with 1 fanins: " << n << "\n";
                        }
                        /*else  // if node has two fanins (or three fanins with one of them being constant)
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

                            node2pos[n] = connect_and_place(layout, prec_pos, ntk, n, pre1_t, pre2_t, fc.constant_fanin);

                            std::cout << "Nodes with 2 fanins: " << n << "\n";
                            // ToDo: new_lines have to be routed after and nodes
                        }*/
                    }
                    ++r;
                });
        }

        /*ctn.color_ntk.foreach_node(
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
                                    node2pos[n] =
                                        layout.create_buf(wire_east(layout, {0, latest_pos.y}, latest_pos), latest_pos);
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
                            // make sure pre1_t is the northwards tile and pre2_t is the westwards one
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
                            // make sure pre1_t is the northwards tile and pre2_t is the westwards one
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

                    if (ctn.color_ntk.is_po(n) && (!is_eastern_po_orientation_available(ctn, n) ||
                                                   std::find(multi_output_nodes.cbegin(), multi_output_nodes.cend(),
                                                             n) != multi_output_nodes.cend()))
                    {
                        ++latest_pos.y;
                    }
                }

#if (PROGRESS_BARS)
                // update progress
                bar(i);
#endif
            });*/

        // place outputs after the main algorithm to handle possible multi-output or unordered nodes
        // place_outputs(layout, ctn, po_counter, node2pos);

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
    extended_rank_view<mockturtle::fanout_view<mockturtle::names_view<technology_network>>> ntk;

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

#endif  // FICTION_ORTHOGONAL_PLANAR_HPP
