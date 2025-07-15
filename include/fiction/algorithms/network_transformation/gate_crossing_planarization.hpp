//
// Created by benjamin on 30.06.2025
//

#ifndef FICTION_GATE_CROSSING_PLANARIZATION_HPP
#define FICTION_GATE_CROSSING_PLANARIZATION_HPP

#include "inverter_substitution.hpp"

#include <mockturtle/utils/node_map.hpp>
#include <mockturtle/views/fanout_view.hpp>
#include <mockturtle/views/topo_view.hpp>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <utility>
#include <vector>
namespace fiction
{

namespace detail
{

template <typename Ntk>
struct Edge
{
    uint32_t           id;
    mockturtle::node<Ntk> src, dst;
};

inline void set_bit(std::vector<uint64_t>& row, uint32_t index)
{
    row[index >> 6u] |= static_cast<uint64_t>(1) << (index & 0x3f);
}
inline void clear_bit(std::vector<uint64_t>& row, uint32_t j)
{
    row[j >> 6] &= ~(1ULL << (j & 63));
}
inline bool test_bit(const std::vector<uint64_t>& row, uint32_t j)
{
    return row[j >> 6] & (1ULL << (j & 63));
}
inline uint32_t popcount_row(const std::vector<uint64_t>& row)
{
    uint32_t s = 0;
    for (uint64_t w : row)
    {
        s += static_cast<uint32_t>(std::__popcount(w));
    }
    return s;
}
inline uint32_t first_set(const std::vector<uint64_t>& row)
{
    for (uint32_t blk = 0; blk < row.size(); ++blk)
    {
        if (row[blk])
        {
            return (blk << 6) + std::__countr_zero(row[blk]);
        }
    }
    return UINT32_MAX;  // no bit set
}

template <typename Ntk>
class insert_planar_crossings_impl
{
  public:
    explicit insert_planar_crossings_impl(const Ntk& ntk_src) : ntk{ntk_src}, fanout_ntk{ntk_src} {}

    void insert_crossing(Ntk& ntk_dest, mockturtle::node_map<mockturtle::signal<Ntk>, Ntk>& old2new,
                         mockturtle::node<Ntk>& left, mockturtle::node<Ntk>& right, std::vector<mockturtle::node<Ntk>> node_ranks, std::vector<uint32_t> node_level)
    {
        // take both nodes as inputs to a three xor gate structure
        const auto fo_left = ntk_dest.create_buf(ntk_dest.make_signal(left));
        ntk_dest.on_add(fo_left);
        const auto fo_right = ntk_dest.create_buf(ntk_dest.make_signal(right));
        ntk_dest.on_add(fo_right);
        const auto xor_mid = ntk_dest.create_xor(fo_left, fo_right);
        ntk_dest.on_add(xor_mid);
        const auto buf_xor_left = ntk_dest.create_buf(fo_left);
        ntk_dest.on_add(buf_xor_left);
        const auto buf_xor_right = ntk_dest.create_buf(fo_right);
        ntk_dest.on_add(buf_xor_right);
        const auto fo_xor_mid = ntk_dest.create_buf(xor_mid);
        ntk_dest.on_add(fo_xor_mid);
        const auto buf_fo_xor_left = ntk_dest.create_buf(buf_xor_left);
        ntk_dest.on_add(buf_fo_xor_left);
        const auto buf_fo_xor_right = ntk_dest.create_buf(buf_xor_right);
        ntk_dest.on_add(buf_fo_xor_right);

        // the swapped signals
        old2new[right] = ntk_dest.create_xor(fo_xor_mid, buf_fo_xor_left);
        ntk_dest.on_add(old2new[right]);
        old2new[left] = ntk_dest.create_xor(fo_xor_mid, buf_fo_xor_right);
        ntk_dest.on_add(old2new[left]);

        // ntk_dest.update_ranks();

        // update the levels and ranks of the nodes after the crossings
        // for each level of crossings save all nodes from the crossings as a matrix
        // three dimensions: [level of crossing][node_depth][node_rank]

        /*if (ntk.is_and(g))
        {
            old2new[g] = ntk_dest.create_and(children[0], children[1]);
            set_level_and_rank(ntk_dest, old2new, g);
            return true;  // keep looping
        }*/

        std::cout << "Inserting Crossing between: " << left << " and: " << right << std::endl;
    }

    auto collect_crossing_matrix(uint32_t r)
    {
        std::vector<std::vector<uint64_t>> matrix{};
        std::vector<Edge<Ntk>>             edges{};

        ntk.foreach_node_in_rank(r,
                                 [this, &edges, &matrix](auto const& src)
                                 {
                                     fanout_ntk.foreach_fanout(src,
                                                        [this, &edges, &matrix, &src](auto const& dst)
                                                        {
                                                            // resize crossing matrix
                                                            const auto     id    = static_cast<uint32_t>(edges.size());
                                                            const uint32_t words = ((id + 64u) >> 6u);
                                                            matrix.emplace_back(words, 0ULL);
                                                            if (id % 64u == 0 && id != 0)
                                                            {
                                                                for (auto& row : matrix)
                                                                {
                                                                    row.push_back(0ULL);
                                                                }
                                                            }

                                                            // update crossing matrix
                                                            const auto src_pos = ntk.rank_position(src);
                                                            const auto dst_pos = ntk.rank_position(dst);
                                                            for (const auto& edge : edges)
                                                            {
                                                                const auto id2      = edge.id;
                                                                const auto src2_pos = ntk.rank_position(edge.src);
                                                                const auto dst2_pos = ntk.rank_position(edge.dst);

                                                                if ((src_pos < src2_pos && dst_pos > dst2_pos) ||
                                                                    (src_pos > src2_pos && dst_pos < dst2_pos))
                                                                {
                                                                    set_bit(matrix[id], id2);
                                                                    set_bit(matrix[id2], id);
                                                                }
                                                            }

                                                            edges.push_back({id, src, dst});
                                                        });
                                 });

        return std::pair{std::move(matrix), std::move(edges)};
    }

    void eliminate_and_insert(Ntk& ntk_dest, mockturtle::node_map<mockturtle::signal<Ntk>, Ntk>&  old2new,
                              std::vector<std::vector<uint64_t>>& matrix, std::vector<Edge<Ntk>>& edges, uint32_t lvl)
    {
        // save the modified depth of the nodes in the level
        std::vector<uint32_t> node_level(ntk.rank_width(lvl));

        // save the original ranks of the level
        auto node_ranks = ntk.get_ranks(lvl);

        // number of crossings per edge
        std::vector<uint32_t> degree(edges.size());
        for (uint32_t i = 0; i < edges.size(); ++i)
        {
            degree[i] = popcount_row(matrix[i]);
        }

        while (true)
        {
            uint32_t best = 0, e = UINT32_MAX;
            for (uint32_t i = 0; i < edges.size(); ++i)
            {
                if (degree[i] > best)
                {
                    best = degree[i];
                    e    = i;
                }
            }
            if (best == 0)
            {
                break;  // done
            }

            // get direction of edge
            const bool left_to_right = edges[e].src < edges[e].dst;

            // if left to right, also resolve crossings left to right
            if (left_to_right)
            {
                // iterate through the words
                for (uint32_t i = 0; i < matrix[e].size(); ++i)
                {
                    auto word = matrix[e][i];
                    // iterate through the word entries
                    while (word != 0u) {
                        auto idx = static_cast<unsigned int>(__builtin_ctzll(word));
                        std::cout << idx << "\n";

                        // insert one crossing at a time
                        insert_crossing(ntk_dest, old2new, edges[e].src, edges[idx].src, node_ranks, node_level);

                        // clear the corresponding matrix entries
                        --degree[idx];
                        --degree[e];
                        clear_bit(matrix[e], idx);
                        clear_bit(matrix[idx], e);

                        word &= word - 1;
                    }
                }
            }
            // if right to left, also resolve crossings right to left
        }
    }

    Ntk run()
    {
        auto  init     = mockturtle::initialize_copy_network<Ntk>(ntk);
        auto& ntk_dest = init.first;
        auto& old2new  = init.second;

        const auto gather_fanin_signals = [this, &ntk_dest, &old2new](const auto& n)
        {
            std::vector<typename Ntk::signal> children{};

            ntk.foreach_fanin(n,
                              [this, &ntk_dest, &old2new, &children](const auto& f)
                              {
                                  const auto fn         = ntk.get_node(f);
                                  const auto tgt_signal = old2new[fn];

                                  children.emplace_back(ntk.is_complemented(f) ? ntk_dest.create_not(tgt_signal) :
                                                                                 tgt_signal);
                              });

            return children;
        };

#if (PROGRESS_BARS)
        // initialize a progress bar
        mockturtle::progress_bar bar{static_cast<uint32_t>(ntk.num_gates()), "[i] network conversion: |{0}|"};
#endif

        if constexpr (mockturtle::has_rank_position_v<Ntk>)
        {
            ntk.foreach_pi([&ntk_dest](const auto& pi) { ntk_dest.on_add(pi); });
        }

        for (uint32_t lvl = 0; lvl < ntk.depth() + 1; ++lvl)
        {
            // resolve crossings
            if ( lvl != ntk.depth())
            {
                // collect the edge crossing matrix
                auto mm     = collect_crossing_matrix(lvl);
                auto matrix = mm.first;
                auto edges  = mm.second;

                // insert crossings based on the edge crossing matrix
                eliminate_and_insert(ntk_dest, old2new, matrix, edges, lvl);

                // resolve the final connections
            }

            if (lvl == 0)
            {
                continue;
            }

            // insert nodes from next level
            ntk.foreach_node_in_rank(lvl,
                                     [&, this](const auto& g, [[maybe_unused]] auto i)
                                     {
                                         auto children = gather_fanin_signals(g);

#if (PROGRESS_BARS)
                                         // update progress
                                         bar(i);
#endif

                                         if (connect_children_to_gates_unaffected(ntk, ntk_dest, old2new, g, children))
                                         {
                                             return true;  // keep looping
                                         }

                                         return true;
                                     });
        }

        ntk.foreach_po(
            [this, &ntk_dest, &old2new](const auto& po)
            {
                const auto tgt_signal = old2new[ntk.get_node(po)];
                const auto tgt_po     = ntk.is_complemented(po) ? ntk_dest.create_not(tgt_signal) : tgt_signal;

                ntk_dest.create_po(tgt_po);
            });

        // restore signal names if applicable
        fiction::restore_names(ntk, ntk_dest, old2new);

        return ntk_dest;
    }

  private:
    Ntk ntk{};
    mockturtle::fanout_view<Ntk> fanout_ntk;
};
}  // namespace detail

template <typename Ntk>
Ntk insert_planar_crossings(const Ntk& ntk)
{
    static_assert(mockturtle::is_network_type_v<Ntk>, "Ntk is not a network type");
    static_assert(mockturtle::has_rank_position_v<Ntk>, "Ntk has no ranks");

    assert(ntk.is_combinational() && "Network has to be combinational");

    detail::insert_planar_crossings_impl<Ntk> p{ntk};

    auto result = p.run();

    return result;
}

}  // namespace fiction

#endif  // FICTION_GATE_CROSSING_PLANARIZATION_HPP
