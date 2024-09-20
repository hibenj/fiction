//
// Created by benjamin on 7/29/24.
//

#ifndef FICTION_ORTHOGONAL_PLANAR_HPP
#define FICTION_ORTHOGONAL_PLANAR_HPP

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
                                 if (orientation > 0)
                                 {
                                     return;
                                 }
                                 if (ntk.fanin_size(n) == 2)
                                 {
                                     orientation = 1;
                                     return;
                                 }
                                 if (const auto fc = fanins(ntk, n); fc.fanin_nodes.size() == 1)
                                 {
                                     const auto& pre = fc.fanin_nodes[0];
                                     if (ntk.is_fanout(pre) && ntk.fanout_size(pre) == 2)
                                     {
                                         orientation = 2;
                                         return;
                                     }
                                 }
                             });
    return orientation;
}

template <typename Ntk>
uint32_t propagate_forward(Ntk& ntk, std::vector<std::vector<int>>& fw_gap_array,
                           std::vector<std::vector<int>>& bw_gap_array, uint32_t starting_rank, bool& is_negative,
                           std::vector<int>& orientations)
{
    typename Ntk::node last_visited_node;
    // orientation: 1 = vertical, 2 = horizontal

    uint32_t starting_rank_backward = 0;
    int      fo_gap                 = 0;

    for (uint32_t r = starting_rank; r < ntk.depth(); r++)
    {
        // orientations[r] = start_orientation(ntk, r);
        // std::cout << "Orientation of rank " << r << " is " << orientations[r] << '\n';
        // std::cout << "r: " << r << "\n";
        int orientation = 0;  // start_orientation(ntk, r)
        // identify_structures(ntk);
        ntk.foreach_node_in_rank(
            r,
            [&ntk, &fw_gap_array, &bw_gap_array, &last_visited_node, &r, &orientation, &fo_gap,
             &orientations](const auto& n)
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
                        if (fw_gap_array[r][ntk.rank_position(n)] > 0)
                        {
                            // the last entry holds the new line entry
                            fw_gap_array[r + 1].back() =
                                std::max(fw_gap_array[r + 1].back(), fw_gap_array[r][ntk.rank_position(n)]);

                            // shift gap to the left
                            if (ntk.rank_position(n) != 0)
                            {
                                fw_gap_array[r + 1][ntk.rank_position(fo[1 - max_value_index])] +=
                                    fw_gap_array[r][ntk.rank_position(n)];
                            }
                        }
                    }

                    // check if between the fanouts there is a gap (this gap comes from backward propagation)
                    /*if (fw_gap_array[r + 1][ntk.rank_position(fo[1 - max_value_index])] > 0)
                    {
                        std::cout << "gap between fos\n";
                    }*/

                    if (bw_gap_array[r + 1][ntk.rank_position(fo[1 - max_value_index])] >
                        fw_gap_array[r + 1][ntk.rank_position(fo[1 - max_value_index])])
                    {
                        fw_gap_array[r + 1][ntk.rank_position(fo[1 - max_value_index])] =
                            bw_gap_array[r + 1][ntk.rank_position(fo[1 - max_value_index])];
                    }
                    fo_gap = fw_gap_array[r + 1][ntk.rank_position(fo[1 - max_value_index])];

                    last_visited_node = fo[max_value_index];
                }
                // fi of 2-input node
                else
                {
                    assert(ntk.fanin_size(fo[0]) == 2 && fo.size() == 1);

                    // chain starts here
                    if (last_visited_node != fo[0])
                    {
                        // if array[r, rank(n)] > 0, then x new lines and propagate to left
                        // rank(fo[0]) - 1
                        if (fw_gap_array[r][ntk.rank_position(n)] > 0)
                        {
                            // the last entry holds the new line entry
                            fw_gap_array[r + 1].back() =
                                std::max(fw_gap_array[r + 1].back(), fw_gap_array[r][ntk.rank_position(n)]);

                            // shift gap to the left
                            // if (orientation == 2 && ntk.rank_position(n) != 0)
                            if (ntk.rank_position(n) != 0)
                            {
                                fw_gap_array[r + 1][ntk.rank_position(fo[0]) - 1] +=
                                    fw_gap_array[r][ntk.rank_position(n)];
                            }
                        }

                        if (orientation == 2 && ntk.rank_position(n) != 0)
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
                }
            });
        // compare for new lines
        fw_gap_array[r + 1].back() = std::max(fw_gap_array[r + 1].back(), bw_gap_array[r + 1].back());
        // resolve multi output pos
        if (r == ntk.depth() - 1)
        {
            const auto                                  save_old = fw_gap_array[r + 1];
            std::unordered_map<typename Ntk::node, int> node_counts;

            ntk.foreach_po([&node_counts](auto po) { node_counts[po]++; });

            ntk.foreach_node_in_rank(r + 1,
                                     [&ntk, &node_counts, &fw_gap_array, &r](const auto& n)
                                     {
                                         if (ntk.is_fanout(n))
                                         {
                                             assert(ntk.is_po(n) && node_counts[n] == 2);
                                             fw_gap_array[r + 1][ntk.rank_position(n)] -= 1;
                                         }
                                     });
            is_negative = false;
            for (std::size_t i = 0; i < fw_gap_array[r + 1].size() - 1; i++)
            {
                if (fw_gap_array[r + 1][i] < 0)
                {
                    is_negative = true;
                    break;
                }
            }
            if (!is_negative)
            {
                fw_gap_array[r + 1] = save_old;
            }
        }
        // if there is a negative gap, go to backward propagation
        is_negative = false;
        for (std::size_t i = 0; i < fw_gap_array[r + 1].size() - 1; i++)
        {
            if (fw_gap_array[r + 1][i] < 0)
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
    typename Ntk::node last_visited_node;
    // orientation: 1 = vertical, 2 = horizontal

    uint32_t starting_rank_forward = 0;

    assert(starting_rank < ntk.depth());

    for (uint32_t r = starting_rank; r <= ntk.depth(); r--)
    {
        // resolve multi output pos
        if (r == ntk.depth() - 1)
        {
            std::unordered_map<typename Ntk::node, int> node_counts;

            ntk.foreach_po([&node_counts](auto po) { node_counts[po]++; });

            ntk.foreach_node_in_rank(r + 1,
                                     [&ntk, &node_counts, &bw_gap_array, &r](const auto& n)
                                     {
                                         if (ntk.is_fanout(n))
                                         {
                                             assert(ntk.is_po(n) && node_counts[n] == 2);
                                             bw_gap_array[r + 1][ntk.rank_position(n)] += 1;
                                         }
                                     });
        }
        int orientation = 0;
        ntk.foreach_node_in_rank(
            r,
            [&ntk, &bw_gap_array, &fw_gap_array, &last_visited_node, &r, &orientation](const auto& n)
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
                    if (bw_gap_array[r + 1][ntk.rank_position(fo[1 - max_value_index])] > 0)
                    {
                        // the last entry holds the new line entry
                        bw_gap_array[r + 1].back() =
                            std::max(fw_gap_array[r + 1].back(),
                                     bw_gap_array[r + 1][ntk.rank_position(fo[1 - max_value_index])]);

                        // shift gap to the right
                        if (ntk.rank_position(n) != ntk.rank_width(r) - 1 &&
                            ntk.is_fanout(ntk.at_rank_position(r, ntk.rank_position(n) + 1)))
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
                        if (fw_gap_array[r][ntk.rank_position(n) - 1] > 0)
                        {
                            // shift gap from fw_propagation to the left
                            // if (orientation == 2 && ntk.rank_position(n) != 0)
                            if (orientation != 0 && ntk.rank_position(n) != 0)
                            {
                                bw_gap_array[r][ntk.rank_position(n) - 1] -= fw_gap_array[r][ntk.rank_position(n)];
                            }
                        }
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

        uint32_t starting_rank_forward  = 0;
        uint32_t starting_rank_backward = 0;

        std::vector<std::vector<int>> forward_gap_array(ntk.depth() + 1);
        fill_gap_array_zeros(ntk, forward_gap_array);

        std::vector<std::vector<int>> backward_gap_array(ntk.depth() + 1);
        fill_gap_array_zeros(ntk, backward_gap_array);

        // propagate forward
        // if a gap is negative: return this level as starting_rank_backward
        // set every gap in this line to 0 and propagate backwards
        // stop backward propagation if there is no backwards gap in the level, that is bigger than the
        // corresponding: return this level again as starting_rank_forward start again with forward propagation

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
            // if starting rank = 0, PI gaps from bw_gap_array
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
        }

        // Debug Output
        /*for (int i = 0; i < forward_gap_array.size(); i++)
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
        }*/

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

        const auto first_pi_node = [this](const auto& gaps)
        {
            int level_size = 0;

            level_size = std::accumulate(gaps.begin(), gaps.end() - 1, 0);

            level_size += ntk.num_pis();

            return level_size - 1;
        };

        tile<Lyt> prec_pos{0, 0};
        auto      pi_sz     = first_pi_node(forward_gap_array[0]);
        tile<Lyt> first_pos = {pi_sz, 0};

        for (uint32_t lvl = 0; lvl < ntk.depth() + 1; lvl++)
        {
            auto        level_gaps  = forward_gap_array[lvl];
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
                                int gap     = level_gaps[r - 1];
                                prec_pos    = {prec_pos.x - gap - 1, prec_pos.y + gap + 1};
                                // node2pos[n] = layout.move_node(pi2node[n], prec_pos);
                                if(prec_pos.x == 0)
                                {
                                    node2pos[n] = layout.move_node(pi2node[n], prec_pos);
                                }
                                else if(prec_pos.x < (first_pos.x / 2))
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
        int add_line = 0;
        // the number of outputs on a node is limited to 2, due to fanout substitution
        ntk.foreach_po(
            [&](const auto& po)
            {
                if (!ntk.is_constant(po))
                {
                    const auto      n_s     = node2pos[po];
                    auto            po_tile = static_cast<tile<Lyt>>(n_s);
                    if (countMap[po] < 2) // Check if the count is less than 2
                    {
                        // Adjust the position based on whether it's the first or second occurrence
                        if (countMap[po] == 1) {
                            if(po_tile.y == prec_pos.y)
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

#endif  // FICTION_ORTHOGONAL_PLANAR_HPP
