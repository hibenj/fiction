//
// Created by benjamin on 25.04.23.
//

#ifndef FICTION_ORTHO_MAJORITY_GATES_NETWORK_HPP
#define FICTION_ORTHO_MAJORITY_GATES_NETWORK_HPP

#include "fiction/algorithms/network_transformation/fanout_substitution.hpp"
#include "fiction/algorithms/physical_design/orthogonal.hpp"
#include "fiction/io/print_layout.hpp"
#include "fiction/layouts/clocking_scheme.hpp"
#include "fiction/networks/views/edge_color_view.hpp"
#include "fiction/traits.hpp"
#include "fiction/utils/name_utils.hpp"
#include "fiction/utils/network_utils.hpp"
#include "fiction/utils/placement_utils.hpp"
#include "ortho_ordering_network.hpp"

#include <fmt/format.h>
#include <mockturtle/traits.hpp>
#include <mockturtle/utils/node_map.hpp>
#include <mockturtle/utils/stopwatch.hpp>
#include <mockturtle/views/fanout_view.hpp>
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
namespace detail
{
/**
 * Specific functions for the placement of majority gates
 */
template <typename Lyt>
mockturtle::signal<Lyt> wire_west(Lyt& lyt, const tile<Lyt>& src, const tile<Lyt>& dest)
{
    auto a = static_cast<mockturtle::signal<Lyt>>(src);

    if (src.x - 1 == dest.x && dest.x == 0)
    {
        auto t = tile<Lyt>{dest.x, src.y, 0};
        if (!lyt.is_empty_tile(t))  // crossing case
        {
            t = lyt.above(t);
        }

        a = lyt.create_buf(a, t);
    }

    for (auto x = src.x - 1; x > dest.x; --x)
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
mockturtle::signal<Lyt> wire_north(Lyt& lyt, const tile<Lyt>& src, const tile<Lyt>& dest)
{
    auto a = static_cast<mockturtle::signal<Lyt>>(src);

    if (src.y - 1 == dest.y && dest.y == 0)
    {
        auto t = tile<Lyt>{src.x, dest.y, 0};
        if (!lyt.is_empty_tile(t))  // crossing case
        {
            t = lyt.above(t);
        }

        a = lyt.create_buf(a, t);
    }

    for (auto y = src.y - 1; y > dest.y; --y)
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
template <typename Lyt>
mockturtle::signal<Lyt> buffer_south(Lyt& layout, const tile<Lyt>& src, unsigned char pre_clock)
{
    auto pre2_t = static_cast<tile<Lyt>>(wire_east(layout, src, {src.x + 2, src.y}));
    layout.assign_clock_number({pre2_t.x, pre2_t.y, 0}, pre_clock + 1);
    pre_clock = layout.get_clock_number({pre2_t});

    pre2_t = static_cast<tile<Lyt>>(wire_south(layout, pre2_t, {pre2_t.x, pre2_t.y + 2}));
    layout.assign_clock_number({pre2_t.x, pre2_t.y, 0}, pre_clock + 1);
    pre_clock = layout.get_clock_number({pre2_t});

    pre2_t = static_cast<tile<Lyt>>(wire_west(layout, pre2_t, {pre2_t.x - 2, pre2_t.y}));
    layout.assign_clock_number({pre2_t.x, pre2_t.y, 0}, pre_clock + 1);
    pre_clock = layout.get_clock_number({pre2_t});

    pre2_t = static_cast<tile<Lyt>>(wire_south(layout, pre2_t, {pre2_t.x, pre2_t.y + 2}));
    layout.assign_clock_number({pre2_t.x, pre2_t.y, 0}, pre_clock + 1);
    pre_clock = layout.get_clock_number({pre2_t});

    pre2_t = static_cast<tile<Lyt>>(wire_east(layout, pre2_t, {pre2_t.x + 2, pre2_t.y}));
    layout.assign_clock_number({pre2_t.x, pre2_t.y, 0}, pre_clock + 1);
    pre_clock = layout.get_clock_number({pre2_t});

    pre2_t = static_cast<tile<Lyt>>(wire_south(layout, pre2_t, {pre2_t.x, pre2_t.y + 2}));
    layout.assign_clock_number({pre2_t.x, pre2_t.y, 0}, pre_clock + 1);
    pre_clock = layout.get_clock_number({pre2_t});

    pre2_t = static_cast<tile<Lyt>>(wire_west(layout, pre2_t, {pre2_t.x - 2, pre2_t.y}));
    layout.assign_clock_number({pre2_t.x, pre2_t.y, 0}, pre_clock + 1);

    pre2_t = static_cast<tile<Lyt>>(wire_south(layout, pre2_t, {pre2_t.x, pre2_t.y + 2}));

    return static_cast<mockturtle::signal<Lyt>>(pre2_t);
}
template <typename Lyt>
mockturtle::signal<Lyt> buffer_east(Lyt& layout, const tile<Lyt>& src, unsigned char pre_clock)
{
    auto pre1_t = static_cast<tile<Lyt>>(wire_south(layout, src, {src.x, src.y + 2}));
    layout.assign_clock_number({pre1_t.x, pre1_t.y, 0}, pre_clock + 1);
    pre_clock = layout.get_clock_number({pre1_t});

    pre1_t = static_cast<tile<Lyt>>(wire_east(layout, pre1_t, {pre1_t.x + 2, pre1_t.y}));
    layout.assign_clock_number({pre1_t.x, pre1_t.y, 0}, pre_clock + 1);
    pre_clock = layout.get_clock_number({pre1_t});
    // workaround for the case that the y-coordinate is 0
    if (pre1_t.y == 1)
    {
        pre1_t = static_cast<tile<Lyt>>(wire_north(layout, pre1_t, {pre1_t.x, pre1_t.y - 1}));
        layout.assign_clock_number({pre1_t.x, pre1_t.y, 0}, pre_clock + 1);
        pre_clock = layout.get_clock_number({pre1_t});
    }
    else
    {
        pre1_t = static_cast<tile<Lyt>>(wire_north(layout, pre1_t, {pre1_t.x, pre1_t.y - 2}));
        layout.assign_clock_number({pre1_t.x, pre1_t.y, 0}, pre_clock + 1);
        pre_clock = layout.get_clock_number({pre1_t});
    }

    pre1_t = static_cast<tile<Lyt>>(wire_east(layout, pre1_t, {pre1_t.x + 2, pre1_t.y}));
    layout.assign_clock_number({pre1_t.x, pre1_t.y, 0}, pre_clock + 1);
    pre_clock = layout.get_clock_number({pre1_t});

    pre1_t = static_cast<tile<Lyt>>(wire_south(layout, pre1_t, {pre1_t.x, pre1_t.y + 2}));
    layout.assign_clock_number({pre1_t.x, pre1_t.y, 0}, pre_clock + 1);
    pre_clock = layout.get_clock_number({pre1_t});

    pre1_t = static_cast<tile<Lyt>>(wire_east(layout, pre1_t, {pre1_t.x + 2, pre1_t.y}));
    layout.assign_clock_number({pre1_t.x, pre1_t.y, 0}, pre_clock + 1);
    pre_clock = layout.get_clock_number({pre1_t});

    if (pre1_t.y == 1)
    {
        pre1_t = static_cast<tile<Lyt>>(wire_north(layout, pre1_t, {pre1_t.x, pre1_t.y - 1}));
        layout.assign_clock_number({pre1_t.x, pre1_t.y, 0}, pre_clock + 1);
        pre_clock = layout.get_clock_number({pre1_t});
    }
    else
    {
        pre1_t = static_cast<tile<Lyt>>(wire_north(layout, pre1_t, {pre1_t.x, pre1_t.y - 2}));
        layout.assign_clock_number({pre1_t.x, pre1_t.y, 0}, pre_clock + 1);
        pre_clock = layout.get_clock_number({pre1_t});
    }

    pre1_t = static_cast<tile<Lyt>>(wire_east(layout, pre1_t, {pre1_t.x + 2, pre1_t.y}));
    layout.assign_clock_number({pre1_t.x, pre1_t.y, 0}, pre_clock + 1);

    return static_cast<mockturtle::signal<Lyt>>(pre1_t);
}
// provides the connect_and_place function for majority gates (having three predecessors)
template <typename Lyt, typename Ntk>
mockturtle::signal<Lyt> connect_and_place(Lyt& lyt, const tile<Lyt>& t, const Ntk& ntk, const mockturtle::node<Ntk>& n,
                                          tile<Lyt> pre1_t, tile<Lyt> pre2_t, tile<Lyt> pre3_t)
{
    // pre1_t is northern tile, pre2_t is western tile, pre3_t is eastern tile
    if (pre2_t.y < pre1_t.y)
    {
        std::swap(pre1_t, pre2_t);
    }
    if (pre3_t.y < pre1_t.y)
    {
        std::swap(pre1_t, pre3_t);
    }
    if (pre3_t.x < pre2_t.x)
    {
        std::swap(pre2_t, pre3_t);
    }

    return place(lyt, t, ntk, n, wire_south(lyt, pre1_t, t), wire_east(lyt, pre2_t, t), wire_west(lyt, pre3_t, t));
}
// check edges for buffering according to the delays resulting from majority gates
template <typename Ntk>
std::vector<int> majority_buffer(const Ntk& ntk, mockturtle::node<Ntk> n) noexcept
{
    const auto                  fc   = fanins(ntk, n);
    const auto                  size = fc.fanin_nodes.size();
    std::vector<int>            delays(size);
    std::vector<int>::size_type iterator{0};

    if (size != 0)
    {
        foreach_incoming_edge(ntk, n,
                              [&delays, &ntk, &iterator](const auto& e)
                              {
                                  if (!ntk.is_constant(e.source))
                                  {
                                      auto node_paths = all_incoming_edge_paths(ntk, e.source);

                                      for (std::size_t i = 0; i < node_paths.size(); ++i)
                                      {
                                          int path_delay = 0;
                                          for (int j = 0; j < node_paths[i].size(); ++j)
                                          {
                                              if (const auto ft = fanins(ntk, node_paths[i][j].target);
                                                  ntk.is_maj(node_paths[i][j].target) && ft.fanin_nodes.size() > 2)
                                              {
                                                  ++path_delay;
                                              }
                                          }
                                          if (delays[iterator] < path_delay)
                                          {
                                              delays[iterator] = path_delay;
                                          }
                                      }
                                      ++iterator;
                                  }
                              });

        const int max = *std::max_element(delays.begin(), delays.end());
        for (int& delay : delays)
        {
            delay = max - delay;
        }
    }

    return delays;
}

template <typename Lyt, typename Ntk>
aspect_ratio<Lyt> determine_layout_size_majority(const coloring_container<Ntk>& ctn) noexcept
{
#if (PROGRESS_BARS)
    // initialize a progress bar
    mockturtle::progress_bar bar{static_cast<uint32_t>(ctn.color_ntk.size()), "[i] determining layout size: |{0}|"};
#endif

    uint64_t x = 0ull, y = ctn.color_ntk.num_pis() - 1;
    uint64_t color_null_wired_south = 0ull;
    ctn.color_ntk.foreach_node(
        [&](const auto& n, [[maybe_unused]] const auto i)
        {
            if (!ctn.color_ntk.is_constant(n))
            {
                const auto fos = fanouts(ctn.color_ntk, n);
                if (ctn.color_ntk.is_fanout(n) && fos.size() > 1)
                {
                    // for majority gates colored null
                    if ((ctn.color_ntk.is_maj(fos[0]) && ctn.color_ntk.color(fos[0]) == ctn.color_null &&
                         ctn.color_ntk.color(fos[1]) == ctn.color_east) ||
                        (ctn.color_ntk.is_maj(fos[1]) && ctn.color_ntk.color(fos[1]) == ctn.color_null &&
                         ctn.color_ntk.color(fos[0]) == ctn.color_east))
                    {
                        ++color_null_wired_south;
                    }
                }
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
                else if (const auto fc = fanins(ctn.color_ntk, n); ctn.color_ntk.is_maj(n) && fc.fanin_nodes.size() > 2)
                {
                    const auto maj_buf = majority_buffer(ctn.color_ntk, n);
                    for (auto& m : maj_buf)
                    {
                        if (m > 0)
                        {
                            x = x + m * 4 + 1;
                        }
                    }
                    if (const auto maj_clr = ctn.color_ntk.color(n); maj_clr == ctn.color_south)
                    {
                        y = y + 3;
                    }
                    x = x + 5;
                    y = y + 5;
                }
                else if (const auto clr = ctn.color_ntk.color(n); clr == ctn.color_east)
                {
                    const auto maj_buf = majority_buffer(ctn.color_ntk, n);
                    for (auto& m : maj_buf)
                    {
                        if (m > 0)
                        {
                            x = x + m * 4 + 1;
                            y = y + 3;
                        }
                    }
                    ++x;
                }
                else if (clr == ctn.color_south)
                {
                    const auto maj_buf = majority_buffer(ctn.color_ntk, n);
                    for (auto& m : maj_buf)
                    {
                        if (m > 0)
                        {
                            y = y + m * 4 + 1;
                            x = x + 3;
                        }
                    }
                    ++y;
                }
                else if (clr == ctn.color_null)
                {
                    const auto maj_buf = majority_buffer(ctn.color_ntk, n);
                    for (auto& m : maj_buf)
                    {
                        if (m > 0)
                        {
                            y = y + m * 4 + 1;
                            x = x + 3;
                        }
                    }
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
    y = y + color_null_wired_south;

    return {x, y, 1};
}
// route the predecessors of majority gates
template <typename Ntk, typename Lyt>
int check_maj_color(Lyt layout, const coloring_container<Ntk>& ctn,
                    mockturtle::node_map<mockturtle::signal<Lyt>, decltype(ctn.color_ntk)>& node2pos,
                    tile<Lyt>& latest_pos, const mockturtle::node<Ntk> n, const mockturtle::node<Ntk> pre,
                    tile<Lyt>& pre_t)
{
    if (ctn.color_ntk.color(n) == ctn.color_east)
    {
        pre_t = static_cast<tile<Lyt>>(wire_east(layout, pre_t, {latest_pos.x + 1, pre_t.y}));
        return 0;
    }
    if (ctn.color_ntk.color(n) == ctn.color_south)
    {
        pre_t = static_cast<tile<Lyt>>(wire_south(layout, pre_t, {pre_t.x, latest_pos.y + 1}));
        ++latest_pos.y;
        return 1;
    }
    pre_t = static_cast<tile<Lyt>>(node2pos[pre]);
    if (ctn.color_ntk.is_po(pre))
    {
        pre_t = static_cast<tile<Lyt>>(wire_south(layout, pre_t, {pre_t.x, latest_pos.y + 1}));
        ++latest_pos.y;
        return 1;
    }
    if (const auto fos = fanouts(ctn.color_ntk, pre); ctn.color_ntk.is_fanout(pre) && fos.size() > 1)
    {
        auto conflicting_colors_one = ctn.color_null;
        if (fos[0] != n)
        {
            conflicting_colors_one = ctn.color_ntk.color(fos[0]);
        }
        if (fos[1] != n)
        {
            conflicting_colors_one = ctn.color_ntk.color(fos[1]);
        }
        if (conflicting_colors_one == ctn.color_east)
        {
            pre_t = static_cast<tile<Lyt>>(wire_south(layout, pre_t, {pre_t.x, latest_pos.y + 1}));
            ++latest_pos.y;
            return 1;
        }
        if (conflicting_colors_one == ctn.color_south)
        {
            pre_t = static_cast<tile<Lyt>>(wire_east(layout, pre_t, {latest_pos.x + 1, pre_t.y}));
            return 0;
        }
        assert(false);
    }
}
void compare_and_swap(fiction::offset::ucoord_t& pre1_t, fiction::offset::ucoord_t& pre2_t,
                      fiction::offset::ucoord_t& pre3_t, std::vector<int>& maj_buf)
{
    if (pre2_t.y < pre1_t.y)
    {
        std::swap(pre1_t, pre2_t);
        std::swap(maj_buf[0], maj_buf[1]);
    }
    if (pre3_t.y < pre1_t.y)
    {
        std::swap(pre1_t, pre3_t);
        std::swap(maj_buf[0], maj_buf[2]);
    }
    if (pre3_t.y < pre2_t.y)
    {
        std::swap(pre2_t, pre3_t);
        std::swap(maj_buf[1], maj_buf[2]);
    }
}
template <typename Lyt>
void place_and_route_majority_buffer_east(Lyt layout, const std::vector<int>& maj_buf,
                                          fiction::offset::ucoord_t& pre1_t, fiction::offset::ucoord_t& pre2_t,
                                          tile<Lyt>& t, tile<Lyt>& latest_pos)
{
    for (std::size_t path_n = 0; path_n < maj_buf.size(); ++path_n)
    {
        if (maj_buf[path_n] >= 1)
        {
            if (path_n == 0)
            {
                pre2_t = static_cast<tile<Lyt>>(wire_east(layout, pre2_t, {latest_pos.x, pre2_t.y}));
                pre1_t = static_cast<tile<Lyt>>(wire_east(layout, pre1_t, {pre2_t.x + 2, pre1_t.y}));
                pre1_t = static_cast<tile<Lyt>>(wire_south(layout, pre1_t, {pre1_t.x, latest_pos.y + 1}));
                pre2_t = static_cast<tile<Lyt>>(wire_south(layout, pre2_t, {pre2_t.x, pre1_t.y + 3}));

                auto pre_clock = layout.get_clock_number({pre1_t});
                for (int iter = maj_buf[path_n]; iter > 0; --iter)
                {
                    pre1_t = static_cast<tile<Lyt>>(buffer_east(layout, pre1_t, pre_clock));
                }

                t = {pre1_t.x, pre2_t.y};

                latest_pos.y = t.y + 1;
                latest_pos.x = t.x + 1;
            }
            else
            {
                pre2_t = static_cast<tile<Lyt>>(wire_east(layout, pre2_t, {latest_pos.x, pre2_t.y}));
                pre1_t = static_cast<tile<Lyt>>(wire_east(layout, pre1_t, {pre2_t.x + 2, pre1_t.y}));
                pre1_t = static_cast<tile<Lyt>>(wire_south(layout, pre1_t, {pre1_t.x, latest_pos.y + 1}));
                pre2_t = static_cast<tile<Lyt>>(wire_south(layout, pre2_t, {pre2_t.x, pre1_t.y + 2}));

                auto pre_clock = layout.get_clock_number({pre2_t});
                for (int iter = maj_buf[path_n]; iter > 0; --iter)
                {
                    pre2_t = static_cast<tile<Lyt>>(buffer_east(layout, pre2_t, pre_clock));
                }

                pre1_t = static_cast<tile<Lyt>>(wire_east(layout, pre1_t, {pre2_t.x + 2, pre1_t.y}));

                t = {pre1_t.x, pre2_t.y};

                latest_pos.y = t.y + 2;
                latest_pos.x = t.x + 1;
            }
        }
    }
}

template <typename Lyt>
void place_and_route_majority_buffer_south(Lyt layout, const std::vector<int>& maj_buf,
                                           fiction::offset::ucoord_t& pre1_t, fiction::offset::ucoord_t& pre2_t,
                                           tile<Lyt>& t, tile<Lyt>& latest_pos)
{
    for (std::size_t path_n = 0; path_n < maj_buf.size(); ++path_n)
    {
        if (maj_buf[path_n] >= 1)
        {
            if (path_n == 0)
            {
                pre1_t = static_cast<tile<Lyt>>(wire_south(layout, pre1_t, {pre1_t.x, latest_pos.y}));
                pre2_t = static_cast<tile<Lyt>>(wire_south(layout, pre2_t, {pre2_t.x, pre1_t.y + 2}));
                pre2_t = static_cast<tile<Lyt>>(wire_east(layout, pre2_t, {latest_pos.x + 1, pre2_t.y}));
                pre1_t = static_cast<tile<Lyt>>(wire_east(layout, pre1_t, {pre2_t.x + 2, pre1_t.y}));

                auto pre_clock = layout.get_clock_number({pre1_t});
                for (int iter = maj_buf[path_n]; iter > 0; --iter)
                {
                    pre1_t = static_cast<tile<Lyt>>(buffer_south(layout, pre1_t, pre_clock));
                }

                pre2_t = static_cast<tile<Lyt>>(wire_south(layout, pre2_t, {pre2_t.x, pre1_t.y + 2}));

                t = {pre1_t.x, pre2_t.y};

                latest_pos.y = t.y + 1;
                latest_pos.x = t.x + 2;
            }
            else
            {
                pre1_t = static_cast<tile<Lyt>>(wire_south(layout, pre1_t, {pre1_t.x, latest_pos.y}));
                pre2_t = static_cast<tile<Lyt>>(wire_south(layout, pre2_t, {pre2_t.x, pre1_t.y + 2}));
                pre2_t = static_cast<tile<Lyt>>(wire_east(layout, pre2_t, {latest_pos.x + 1, pre2_t.y}));
                pre1_t = static_cast<tile<Lyt>>(wire_east(layout, pre1_t, {pre2_t.x + 3, pre1_t.y}));

                auto pre_clock = layout.get_clock_number({pre2_t});
                for (int iter = maj_buf[path_n]; iter > 0; --iter)
                {
                    pre2_t = static_cast<tile<Lyt>>(buffer_south(layout, pre2_t, pre_clock));
                }

                t            = {pre1_t.x, pre2_t.y};
                latest_pos.y = t.y + 1;
                latest_pos.x = t.x + 1;
            }
        }
    }
}
// actual placement and routing of the majority gates including buffers
template <typename Ntk, typename Lyt>
void place_and_route_majority_gate(Lyt layout, const coloring_container<Ntk>& ctn,
                                   mockturtle::node_map<mockturtle::signal<Lyt>, decltype(ctn.color_ntk)>& node2pos,
                                   const mockturtle::node<Ntk>& n, tile<Lyt>& t, tile<Lyt>& ref_point,
                                   tile<Lyt>& latest_pos, fiction::offset::ucoord_t& pre1_t,
                                   fiction::offset::ucoord_t& pre2_t, fiction::offset::ucoord_t& pre3_t,
                                   const std::vector<int>& maj_buf)
{
    // buffers
    for (auto path_n = maj_buf.size(); path_n != 0; --path_n)
    {
        if (maj_buf[path_n - 1] >= 1)
        {
            if (path_n == 3)
            {
                pre3_t = static_cast<tile<Lyt>>(wire_east(layout, pre3_t, {ref_point.x + 1, pre3_t.y}));

                auto pre_clock = layout.get_clock_number({pre3_t});
                for (int iter = maj_buf[path_n - 1]; iter > 0; --iter)
                {
                    pre3_t = static_cast<tile<Lyt>>(buffer_east(layout, pre3_t, pre_clock));
                }

                latest_pos.x = pre3_t.x + 1;

                // CASE the other buffers also
                if (maj_buf[1] > 0 || maj_buf[0] > 0)
                {
                    pre3_t = static_cast<tile<Lyt>>(wire_south(layout, pre3_t, {pre3_t.x, pre3_t.y + 2}));

                    auto local_resolve_row = pre3_t.x + 1;
                    if (maj_buf[1] == 0)
                    {
                        pre2_t = static_cast<tile<Lyt>>(wire_east(layout, pre2_t, {local_resolve_row + 1, pre2_t.y}));
                        pre2_t = static_cast<tile<Lyt>>(wire_south(layout, pre2_t, {pre2_t.x, pre2_t.y + 2}));
                        ++latest_pos.x;
                    }
                }
            }
            else if (path_n == 2)
            {
                // For this case we need RESOLVE for nodes getting wired east but are blocked by the Buffer
                pre2_t = static_cast<tile<Lyt>>(wire_east(layout, pre2_t, {ref_point.x + 1, pre2_t.y}));

                auto pre_clock = layout.get_clock_number({pre2_t});
                for (int iter = maj_buf[path_n - 1]; iter > 0; --iter)
                {
                    pre2_t = static_cast<tile<Lyt>>(buffer_east(layout, pre2_t, pre_clock));
                }
                pre2_t = static_cast<tile<Lyt>>(wire_south(layout, pre2_t, {pre2_t.x, pre2_t.y + 2}));

                if (maj_buf[2] == 0)
                {
                    pre3_t = static_cast<tile<Lyt>>(wire_south(layout, pre3_t, {pre3_t.x, pre3_t.y + 2}));
                }

                latest_pos.x = pre2_t.x + 1;
            }
            else
            {
                pre1_t = static_cast<tile<Lyt>>(wire_east(layout, pre1_t, {ref_point.x + 1, pre1_t.y}));

                auto pre_clock = layout.get_clock_number({pre1_t});
                for (int iter = maj_buf[path_n - 1]; iter > 0; --iter)
                {
                    pre1_t = static_cast<tile<Lyt>>(buffer_east(layout, pre1_t, pre_clock));
                }
                pre1_t = static_cast<tile<Lyt>>(wire_south(layout, pre1_t, {pre1_t.x, pre1_t.y + 2}));

                latest_pos.x = pre1_t.x + 1;

                // because buffer is 2 tiles wide in y-direction
                ++latest_pos.y;
            }
            ref_point.x = latest_pos.x;
        }
    }

    // route majority gates to new columns and rows
    pre1_t = static_cast<tile<Lyt>>(wire_east(layout, pre1_t, {ref_point.x + 4, pre1_t.y}));
    pre2_t = static_cast<tile<Lyt>>(wire_east(layout, pre2_t, {ref_point.x + 3, pre2_t.y}));
    pre3_t = static_cast<tile<Lyt>>(wire_east(layout, pre3_t, {ref_point.x + 1, pre3_t.y}));

    pre1_t = static_cast<tile<Lyt>>(wire_south(layout, pre1_t, {pre1_t.x, ref_point.y + 3}));
    pre2_t = static_cast<tile<Lyt>>(wire_south(layout, pre2_t, {pre2_t.x, ref_point.y + 1}));
    pre3_t = static_cast<tile<Lyt>>(wire_south(layout, pre3_t, {pre3_t.x, ref_point.y + 4}));

    // Delay Network
    auto pre_clock = layout.get_clock_number({pre2_t});
    pre2_t         = static_cast<tile<Lyt>>(wire_west(layout, pre2_t, {pre2_t.x - 2, pre2_t.y}));
    layout.assign_clock_number({pre2_t.x, pre2_t.y, 0}, pre_clock + 1);
    pre_clock = layout.get_clock_number({pre2_t});
    pre2_t    = static_cast<tile<Lyt>>(wire_south(layout, pre2_t, {pre2_t.x, pre2_t.y + 2}));
    layout.assign_clock_number({pre2_t.x, pre2_t.y, 0}, pre_clock + 1);
    pre_clock = layout.get_clock_number({pre2_t});
    pre2_t    = static_cast<tile<Lyt>>(wire_east(layout, pre2_t, {pre2_t.x + 2, pre2_t.y}));
    layout.assign_clock_number({pre2_t.x, pre2_t.y, 0}, pre_clock + 1);
    pre3_t    = static_cast<tile<Lyt>>(wire_east(layout, pre3_t, {pre3_t.x + 2, pre3_t.y}));
    pre_clock = layout.get_clock_number({pre3_t});
    pre3_t    = static_cast<tile<Lyt>>(wire_north(layout, pre3_t, {pre3_t.x, pre3_t.y - 2}));
    layout.assign_clock_number({pre3_t.x, pre3_t.y, 0}, pre_clock + 1);
    pre_clock = layout.get_clock_number({pre3_t});

    // coordinates of the majority gate
    t = {ref_point.x + 2, ref_point.y + 2};

    // placement of the majority gate
    node2pos[n] = connect_and_place(layout, t, ctn.color_ntk, n, pre2_t, pre3_t, pre1_t);
    layout.assign_clock_number({t.x, t.y, 0}, pre_clock + 1);

    // route majority gate to fit into the 2DDWave scheme
    pre_clock = layout.get_clock_number({t});
    t         = static_cast<tile<Lyt>>(wire_south(layout, t, {t.x, t.y + 3}));
    layout.assign_clock_number({t.x, t.y - 1, 0}, pre_clock + 1);
    layout.assign_clock_number({t.x, t.y, 0}, pre_clock + 2);
    pre_clock = layout.get_clock_number({t});
    t         = static_cast<tile<Lyt>>(wire_east(layout, t, {t.x + 2, t.y}));
    layout.assign_clock_number({t.x, t.y, 0}, pre_clock + 1);
    t = static_cast<tile<Lyt>>(wire_north(layout, t, {t.x, t.y - 2}));
    t = static_cast<tile<Lyt>>(wire_east(layout, t, {t.x + 2, t.y}));

    // adjust the position to the output-wire of the distribution network
    node2pos[n] = static_cast<mockturtle::signal<Lyt>>(t);

    latest_pos.x = t.x + 1;
    latest_pos.y = t.y + 2;
}

template <typename Lyt, typename Ntk>
class orthogonal_majority_network_impl
{
  public:
    orthogonal_majority_network_impl(const Ntk& src, const orthogonal_physical_design_params& p,
                                     orthogonal_physical_design_stats& st) :
            ntk{mockturtle::fanout_view{fanout_substitution<mockturtle::names_view<technology_network>>(src)}},
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
        Lyt layout{determine_layout_size_majority<Lyt>(ctn), twoddwave_clocking<Lyt>(ps.number_of_clock_phases)};

        // reserve PI nodes without positions
        auto pi2node = reserve_input_nodes(layout, ctn.color_ntk);

        // first x-pos to use for gates is 1 because PIs take up the 0th column
        tile<Lyt> latest_pos{1, 0};

#if (PROGRESS_BARS)
        // initialize a progress bar
        mockturtle::progress_bar bar{ctn.color_ntk.size(), "[i] arranging layout: |{0}|"};
#endif

        ctn.color_ntk.foreach_node(
            [&, this](const auto& n, [[maybe_unused]] const auto i)
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
                    else if (const auto fc_m = fanins(ctn.color_ntk, n);
                             ctn.color_ntk.is_maj(n) && fc_m.fanin_nodes.size() > 2)
                    {
                        auto pre1 = fc_m.fanin_nodes[0], pre2 = fc_m.fanin_nodes[1], pre3 = fc_m.fanin_nodes[2];

                        auto pre1_t = static_cast<tile<Lyt>>(node2pos[pre1]),
                             pre2_t = static_cast<tile<Lyt>>(node2pos[pre2]),
                             pre3_t = static_cast<tile<Lyt>>(node2pos[pre3]);

                        tile<Lyt> t{}, t1{}, t2{}, t3{};

                        // look for delayed incoming edges
                        auto maj_buf = majority_buffer(ctn.color_ntk, n);

                        // pre1_t northern tile, pre2_t middle tile, pre3_t southern tile
                        compare_and_swap(pre1_t, pre2_t, pre3_t, maj_buf);

                        // check the color of the majority gate
                        check_maj_color(layout, ctn, node2pos, latest_pos, n, pre3, pre3_t);
                        check_maj_color(layout, ctn, node2pos, latest_pos, n, pre2, pre2_t);
                        check_maj_color(layout, ctn, node2pos, latest_pos, n, pre1, pre1_t);

                        // reference point for the placement of the majority gate
                        auto ref_point = latest_pos;

                        // pre1_t northern tile, pre2_t middle tile, pre3_t southern tile
                        compare_and_swap(pre1_t, pre2_t, pre3_t, maj_buf);

                        // placement and routing of the majority gates distribution network
                        place_and_route_majority_gate(layout, ctn, node2pos, n, t, ref_point, latest_pos, pre1_t,
                                                      pre2_t, pre3_t, maj_buf);
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

                        // look for delayed incoming edges
                        auto maj_buf = majority_buffer(ctn.color_ntk, n);

                        // n is colored east
                        if (const auto clr = ctn.color_ntk.color(n); clr == ctn.color_east)
                        {
                            // make sure pre1_t is the northward tile and pre2_t is the westwards one
                            if (pre2_t.y < pre1_t.y)
                            {
                                std::swap(pre1_t, pre2_t);
                                std::swap(maj_buf[0], maj_buf[1]);
                            }

                            // use larger y position of predecessors
                            t = {latest_pos.x, pre2_t.y};

                            // each 2-input gate has one incoming bent wire
                            pre1_t = static_cast<tile<Lyt>>(wire_east(layout, pre1_t, {t.x + 1, pre1_t.y}));

                            ++latest_pos.x;

                            // place a buffer for every delayed incoming edge
                            place_and_route_majority_buffer_east(layout, maj_buf, pre1_t, pre2_t, t, latest_pos);
                        }
                        // n is colored south
                        else if (clr == ctn.color_south)
                        {
                            // make sure pre1_t is the northward tile and pre2_t is the westward one
                            if (pre2_t.x > pre1_t.x)
                            {
                                std::swap(pre1_t, pre2_t);
                                std::swap(maj_buf[0], maj_buf[1]);
                            }

                            // use larger x position of predecessors
                            t = {pre1_t.x, latest_pos.y};

                            // each 2-input gate has one incoming bent wire
                            pre2_t = static_cast<tile<Lyt>>(wire_south(layout, pre2_t, {pre2_t.x, t.y + 1}));

                            ++latest_pos.y;

                            // place a buffer for every delayed incoming edge
                            place_and_route_majority_buffer_south(layout, maj_buf, pre1_t, pre2_t, t, latest_pos);
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

                            // place a buffer for every delayed incoming edge
                            place_and_route_majority_buffer_south(layout, maj_buf, pre1_t, pre2_t, t, latest_pos);
                        }

                        node2pos[n] = connect_and_place(layout, t, ctn.color_ntk, n, pre1_t, pre2_t, fc.constant_fanin);
                    }

                    // create PO at applicable position
                    if (ctn.color_ntk.is_po(n))
                    {
                        const auto n_s = node2pos[n];

                        tile<Lyt> po_tile{};

                        // determine PO orientation
                        if (is_eastern_po_orientation_available(ctn, n))
                        {
                            po_tile = layout.east(static_cast<tile<Lyt>>(n_s));
                            ++latest_pos.x;
                        }
                        else
                        {
                            po_tile = layout.south(static_cast<tile<Lyt>>(n_s));
                            ++latest_pos.y;
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
                            const auto anker = layout.create_buf(n_s, po_tile);

                            po_tile = layout.eastern_border_of(po_tile);

                            layout.create_po(wire_east(layout, static_cast<tile<Lyt>>(anker), po_tile),
                                             ctn.color_ntk.has_output_name(po_counter) ?
                                                 ctn.color_ntk.get_output_name(po_counter++) :
                                                 fmt::format("po{}", po_counter++),
                                             po_tile);
                        }
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
    mockturtle::topo_view<mockturtle::fanout_view<mockturtle::names_view<technology_network>>> ntk;

    orthogonal_physical_design_params ps;
    orthogonal_physical_design_stats& pst;

    uint32_t po_counter{0};
};

}  // namespace detail

/**
 * A modification of the orthogonal algorithm, which is able to place majority gates. However, this functionality also
 * includes the insertion of buffers to meet the global timing constraint
 *
 * May throw a high_degree_fanin_exception if `ntk` contains any node with a fan-in larger than 3.
 *
 * @tparam Lyt Desired gate-level layout type.
 * @tparam Ntk Network type that acts as specification.
 * @param ntk The network that is to place and route.
 * @param ps Parameters.
 * @param pst Statistics.
 * @return A gate-level layout of type `Lyt` that implements `ntk` as an FCN circuit.
 */
template <typename Lyt, typename Ntk>
Lyt orthogonal_majority_network(const Ntk& ntk, orthogonal_physical_design_params ps = {},
                                orthogonal_physical_design_stats* pst = nullptr)
{
    static_assert(is_gate_level_layout_v<Lyt>, "Lyt is not a gate-level layout");
    static_assert(mockturtle::is_network_type_v<Ntk>,
                  "Ntk is not a network type");  // Ntk is being converted to a topology_network anyway, therefore,
                                                 // this is the only relevant check here

    // check for input degree
    if (has_high_degree_fanin_nodes(ntk, 3))
    {
        throw high_degree_fanin_exception();
    }

    orthogonal_physical_design_stats                   st{};
    detail::orthogonal_majority_network_impl<Lyt, Ntk> p{ntk, ps, st};

    auto result = p.run();

    if (pst)
    {
        *pst = st;
    }

    return result;
}

}  // namespace fiction
#endif  // FICTION_ORTHO_MAJORITY_GATES_NETWORK_HPP
