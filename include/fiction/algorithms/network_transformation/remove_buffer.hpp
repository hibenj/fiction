//
// Created by benjamin on 2/19/26.
//

#ifndef FICTION_REMOVE_BUFFER_HPP
#define FICTION_REMOVE_BUFFER_HPP

#include "fiction/algorithms/network_transformation/crossing_gate_planarization.hpp"
#include "fiction/algorithms/network_transformation/network_balancing.hpp"
#include "fiction/algorithms/network_transformation/network_conversion.hpp"
#include "fiction/networks/views/mutable_rank_view.hpp"
#include "fiction/traits.hpp"
#include "fiction/utils/debug/network_writer.hpp"

#include <mockturtle/traits.hpp>
#include <mockturtle/utils/node_map.hpp>
#include <mockturtle/views/fanout_view.hpp>
#include <mockturtle/views/topo_view.hpp>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <numeric>
#include <unordered_map>
#include <utility>
#include <vector>

#if (PROGRESS_BARS)
#include <mockturtle/utils/progress_bar.hpp>
#endif

namespace fiction
{

/**
 * Parameters for the remove buffer algorithm.
 */
struct remove_buffer_params
{
    bool     planar              = false;
    uint32_t allowed_buff_fanout = 2u;
    bool     insert_again        = false;
    bool     unify_outputs       = true;
};

namespace detail
{

template <typename Ntk>
std::unordered_map<typename Ntk::node, double>
compute_barycenters_level(Ntk const& ntk, std::vector<typename Ntk::node> const& ntk_level)
{
    using node = typename Ntk::node;

    std::unordered_map<node, double> barycenters;
    barycenters.reserve(ntk_level.size());

    for (const auto& n : ntk_level)
    {
        double   sum   = 0.0;
        uint32_t count = 0u;

        ntk.foreach_fanin(n,
                          [&](const auto& f)
                          {
                              const auto fn = ntk.get_node(f);

                              if (ntk.is_constant(fn))
                              {
                                  return;
                              }

                              sum += static_cast<double>(ntk.rank_position(fn));
                              ++count;
                          });

        const double barycenter = (count == 0u) ? 0.0 : (sum / static_cast<double>(count));

        barycenters.emplace(n, barycenter);
    }

    return barycenters;
}

template <typename Ntk>
class remove_buffer_impl
{
  public:
    remove_buffer_impl(const Ntk& ntk, const remove_buffer_params p) : ntk{ntk}, ps{p} {}

    Ntk insert_buffers_planar(const Ntk&                                                deleted_buffer_ntk,
                              const mockturtle::node_map<mockturtle::signal<Ntk>, Ntk>& old2new_old)
    {
        struct edge
        {
            mockturtle::node<Ntk> source;
            mockturtle::node<Ntk> target;

            uint32_t buffer_count;

            bool operator==(edge const& other) const
            {
                return (source == other.source) && (target == other.target);
            }

            // add constructor
            edge(mockturtle::node<Ntk> s, mockturtle::node<Ntk> t, uint32_t bc) : source(s), target(t), buffer_count(bc)
            {}
        };

        mockturtle::fanout_view<Ntk>       fanout_ntk{ntk};
        const mockturtle::fanout_view<Ntk> fanout_ntk_del{deleted_buffer_ntk};

        std::unordered_map<mockturtle::node<Ntk>, std::vector<mockturtle::node<Ntk>>> ordered_fanout_nodes;
        // collect node orderings for all fanout nodes form the old network
        ntk.foreach_node(
            [this, &fanout_ntk, &fanout_ntk_del, &old2new_old, &ordered_fanout_nodes](const auto& n)
            {
                if (fanout_ntk.fanout_size(n) > 1u)
                {
                    std::vector<mockturtle::node<Ntk>> fanout_nodes{};

                    fanout_ntk.foreach_fanout(n,
                                              [&](const auto& f)
                                              {
                                                  bool                  is_po = false;
                                                  mockturtle::node<Ntk> fanout{f};
                                                  while (fanout_ntk.is_buf(fanout) &&
                                                         fanout_ntk.fanout_size(fanout) == 1u)
                                                  {
                                                      if (fanout_ntk.is_po(fanout))
                                                      {
                                                          is_po = true;
                                                          break;
                                                      }
                                                      const auto fos{fanouts(fanout_ntk, fanout)};
                                                      assert(fos.size() == 1u);
                                                      fanout = fos[0];
                                                  }
                                                  if (is_po)
                                                  {
                                                      fanout_nodes.push_back(old2new_old[n]);
                                                      return;
                                                  }
                                                  fanout_nodes.push_back(old2new_old[fanout]);
                                              });

                    ordered_fanout_nodes.emplace(old2new_old[n], std::move(fanout_nodes));
                }
                else if (ntk.is_pi(n) && ntk.fanout_size(n) == 1u && fanout_ntk_del.is_po(n))
                {
                    std:: cout << "idk\n";
                    ordered_fanout_nodes.emplace(old2new_old[n], std::vector<mockturtle::node<Ntk>>{old2new_old[n]});
                }
            });

        // initialize a network copy
        auto init = initialize_copy_virtual_pi_network<Ntk>(deleted_buffer_ntk);

        auto balanced = init.first;
        auto old2new  = init.second;

        std::vector<edge>                                                      last_level{};
        std::vector<std::pair<mockturtle::node<Ntk>, mockturtle::signal<Ntk>>> update_signals{};

        // gather PO levels
        const auto po_levels    = get_po_levels(deleted_buffer_ntk);
        const auto max_po_level = *std::max_element(po_levels.cbegin(), po_levels.cend());

        /*fanout_ntk_del.foreach_pi(
            [&](const auto& pi)
            {
                const auto& fos = fanouts(fanout_ntk_del, pi);
                const auto sz = fanout_ntk_del.fanout_size(pi);
                const auto is_po = fanout_ntk_del.is_po(pi);
                if (fos.size() != 1u)
                {
                    std::cout << "Pi 1: " << pi << "\n";
                }
                if (sz != 1u)
                {
                    std::cout << "Pi 2: " << pi << "\n";
                }
            });*/

        // create first edges
        fanout_ntk_del.foreach_pi(
            [&](const auto& pi)
            {
                const auto root = pi;
                // and push fanout edges to new level
                if (auto it = ordered_fanout_nodes.find(root); it != ordered_fanout_nodes.end())
                {
                    const auto& fos = it->second;
                    for (const auto& f : fos)
                    {
                        // Handle Po Nodes
                        if (f == pi)
                        {
                            auto level_diff = max_po_level - fanout_ntk_del.level(root);
                            if (level_diff == 0u)
                            {
                                assert(false && "The network has depth 0");
                            }
                            --level_diff;
                            edge e_po{root, root, level_diff};
                            last_level.push_back(e_po);
                        }
                        else
                        {
                            auto level_diff = fanout_ntk_del.level(f) - fanout_ntk_del.level(root) - 1;
                            edge e_next{root, f, level_diff};
                            last_level.push_back(e_next);
                        }
                    }
                }
                else
                {
                    const auto& fos = fanouts(fanout_ntk_del, root);
                    assert(fos.size() == 1u);
                    const auto& f          = fos[0];
                    auto        level_diff = fanout_ntk_del.level(f) - fanout_ntk_del.level(root) - 1;
                    edge        e_next{root, f, level_diff};
                    last_level.push_back(e_next);
                }
            });

        auto next_level = last_level;
        while (!next_level.empty())
        {
            last_level = next_level;
            next_level.clear();

            mockturtle::node<Ntk> last_source{};

            for (auto i = 0u; i < last_level.size(); ++i)
            {
                const auto& e = last_level[i];

                // Handle Pos
                if (e.source == e.target)
                {
                    const auto buf = balanced.create_buf(old2new[e.source]);
                    update_signals.emplace_back(e.source, buf);
                    if (e.buffer_count > 0u)
                    {
                        edge e_next{e.source, e.target, e.buffer_count - 1u};
                        next_level.push_back(e_next);
                    }
                    continue;
                }

                if (e.buffer_count == 0u)
                {
                    const auto root = e.target;

                    // if the target of e has two fanins, then the edge following this edge in last_level is the other
                    // edge which is fanin to this node. Just chack that this is true and delete the next edge from
                    // last_level to avoid duplicates in next_level
                    const auto fi_size = fanout_ntk_del.fanin_size(root);
                    if (fi_size > 1u)
                    {
                        assert(i + (fi_size - 1u) < last_level.size());
                        for (uint32_t k = 1u; k < fi_size; ++k)
                        {
                            assert(last_level[i + k].target == root);
                            assert(last_level[i + k].buffer_count == 0u);
                        }
                        i += (fi_size - 1u);
                    }

                    // place target
                    std::vector<typename mockturtle::topo_view<Ntk>::signal> children{};
                    fanout_ntk_del.foreach_fanin(root,
                                                 [this, &deleted_buffer_ntk, &old2new, &children](const auto& f)
                                                 {
                                                     const auto fn    = deleted_buffer_ntk.get_node(f);
                                                     auto       child = old2new[fn];
                                                     children.push_back(child);
                                                 });

                    old2new[root] = balanced.clone_node(deleted_buffer_ntk, root, children);

                    // and push fanout edges to new level
                    if (auto it = ordered_fanout_nodes.find(root); it != ordered_fanout_nodes.end())
                    {
                        const auto& fos = it->second;
                        for (const auto& f : fos)
                        {
                            // Handle Po Nodes
                            if (f == root)
                            {
                                auto level_diff = max_po_level - fanout_ntk_del.level(root);
                                if (level_diff == 0u)
                                {
                                    continue;
                                }
                                --level_diff;
                                edge e_po{root, root, level_diff};
                                next_level.push_back(e_po);
                            }
                            else
                            {
                                auto level_diff = fanout_ntk_del.level(f) - fanout_ntk_del.level(root) - 1;
                                edge e_next{root, f, level_diff};
                                next_level.push_back(e_next);
                            }
                        }
                    }
                    else
                    {
                        if (fanout_ntk_del.is_po(root))
                        {
                            auto level_diff = max_po_level - fanout_ntk_del.level(root);
                            if (level_diff == 0u)
                            {
                                continue;
                            }
                            --level_diff;
                            edge e_po{root, root, level_diff};
                            next_level.push_back(e_po);
                            continue;
                        }

                        const auto& fos = fanouts(fanout_ntk_del, root);
                        assert(fos.size() == 1u);
                        const auto& f          = fos[0];
                        auto        level_diff = fanout_ntk_del.level(f) - fanout_ntk_del.level(root) - 1;
                        edge        e_next{root, f, level_diff};
                        next_level.push_back(e_next);
                    }
                }
                else
                {
                    // If we got the same source, then we buffer a fanout node
                    if (e.source != last_source)
                    {
                        // place buffer and push edge with decremented buffer count to new level
                        const auto buf = balanced.create_buf(old2new[e.source]);
                        update_signals.emplace_back(e.source, buf);
                    }
                    edge e_next{e.source, e.target, e.buffer_count - 1u};
                    next_level.push_back(e_next);

                    last_source = e.source;
                }
            }
            for (const auto& [target, new_sig] : update_signals)
            {
                old2new[target] = new_sig;
            }
            update_signals.clear();
        }

        // add primary outputs to finalize the network
        deleted_buffer_ntk.foreach_po(
            [this, &deleted_buffer_ntk, &old2new, &balanced](const auto& po, auto i)
            {
                const auto tgt_signal = old2new[deleted_buffer_ntk.get_node(po)];

                auto tgt_po = deleted_buffer_ntk.is_complemented(po) ? balanced.create_not(tgt_signal) : tgt_signal;

                balanced.create_po(tgt_po);
            });

        balanced.update_ranks();

        const auto pi_ranks = ntk.get_ranks(0);
        balanced.set_ranks(0, pi_ranks);

        // restore signal names if applicable
        fiction::restore_names(deleted_buffer_ntk, balanced, old2new);

        /*balanced = mockturtle::cleanup_dangling(balanced);
        balanced.update_ranks();*/

        return balanced;
    }

    Ntk run()
    {
        // initialize a network copy
        // auto init = mockturtle::initialize_copy_network<Ntk>(ntk);
        auto init = initialize_copy_virtual_pi_network<Ntk>(ntk);

        auto deleted_buffer_ntk = init.first;
        auto old2new            = init.second;

#if (PROGRESS_BARS)
        // initialize a progress bar
        mockturtle::progress_bar bar{static_cast<uint32_t>(ntk_topo.num_gates()), "[i] fanout substitution: |{0}|"};
#endif

        ntk.foreach_gate(
            [&, this](const auto& n, [[maybe_unused]] auto i)
            {
                // gather children, but jump over buffers
                std::vector<mockturtle::signal<mockturtle::topo_view<Ntk>>> children{};

                if (ntk.is_buf(n) && ntk.fanout_size(n) < ps.allowed_buff_fanout + 1u)
                {
                    old2new[n] = old2new[ntk.get_node(fanins(ntk, n).fanin_nodes.front())];
                    return;
                }

                ntk.foreach_fanin(n,
                                  [this, &old2new, &children, &deleted_buffer_ntk](const auto& f)
                                  {
                                      const auto fn = ntk.get_node(f);
                                      children.push_back(old2new[fn]);
                                  });

                // clone the node with new children according to its depth
                old2new[n] = deleted_buffer_ntk.clone_node(ntk, n, children);

#if (PROGRESS_BARS)
                // update progress
                bar(i);
#endif
            });

        // add primary outputs to finalize the network
        ntk.foreach_po(
            [this, &old2new, &deleted_buffer_ntk](const auto& po)
            {
                const auto po_node    = ntk.get_node(po);
                auto       tgt_signal = old2new[po_node];

                tgt_signal = ntk.is_complemented(po) ? deleted_buffer_ntk.create_not(tgt_signal) : tgt_signal;

                deleted_buffer_ntk.create_po(tgt_signal);
            });

        // restore signal names if applicable
        fiction::restore_names(ntk, deleted_buffer_ntk, old2new);

        std::unordered_map<typename Ntk::node, typename Ntk::node> new2old;
        new2old.reserve(ntk.size());
        ntk.foreach_node(
            [&](const auto& n)
            {
                const auto new_node = deleted_buffer_ntk.get_node(old2new[n]);
                new2old.emplace(new_node, n);
            });

        if constexpr (has_update_ranks_v<Ntk>)
        {
            deleted_buffer_ntk.update_ranks();
            // after the update ranks call all nodes are in the correct level. I can utilize here foreach_node in rank.
            if (ps.planar)
            {
                for (auto lvl = 0; lvl < deleted_buffer_ntk.depth() + 1; ++lvl)
                {
                    if (lvl == 0u)
                    {
                        // level 0: preserve original PI order
                        const auto pi_ranks = ntk.get_ranks(0);
                        deleted_buffer_ntk.set_ranks(0, pi_ranks);

                        continue;
                    }

                    // forward pass: compute barycenters from already-assigned predecessors
                    auto ntk_level = deleted_buffer_ntk.get_ranks(lvl);

                    auto barycenters = compute_barycenters_level(deleted_buffer_ntk, ntk_level);

                    const auto align_to_level = [&](typename Ntk::node n, const uint32_t target_level)
                    {
                        auto current_level = ntk.level(n);
                        while (current_level > target_level)
                        {
                            const auto fanin_nodes = fanins(ntk, n).fanin_nodes;
                            if (fanin_nodes.empty())
                            {
                                break;
                            }
                            n             = fanin_nodes.front();
                            current_level = ntk.level(n);
                        }
                        return n;
                    };

                    std::stable_sort(ntk_level.begin(), ntk_level.end(),
                                     [&](const auto& a, const auto& b)
                                     {
                                         const auto a_it = barycenters.find(a);
                                         const auto b_it = barycenters.find(b);
                                         const auto a_bc = (a_it != barycenters.end()) ? a_it->second : 0.0;
                                         const auto b_bc = (b_it != barycenters.end()) ? b_it->second : 0.0;
                                         if (a_bc != b_bc)
                                         {
                                             return a_bc < b_bc;
                                         }

                                         const auto a_old_it = new2old.find(a);
                                         const auto b_old_it = new2old.find(b);
                                         if (a_old_it == new2old.end() || b_old_it == new2old.end())
                                         {
                                             return a < b;
                                         }

                                         auto       a_old   = a_old_it->second;
                                         auto       b_old   = b_old_it->second;
                                         const auto a_level = ntk.level(a_old);
                                         const auto b_level = ntk.level(b_old);

                                         if (a_level != b_level)
                                         {
                                             const auto target_level = std::min(a_level, b_level);
                                             if (a_level > b_level)
                                             {
                                                 a_old = align_to_level(a_old, target_level);
                                             }
                                             else
                                             {
                                                 b_old = align_to_level(b_old, target_level);
                                             }
                                         }

                                         const auto a_pos = ntk.rank_position(a_old);
                                         const auto b_pos = ntk.rank_position(b_old);
                                         if (a_pos != b_pos)
                                         {
                                             return a_pos < b_pos;
                                         }

                                         return a < b;
                                     });

                    deleted_buffer_ntk.set_ranks(lvl, ntk_level);
                }
            }
        }

        if (ps.insert_again)
        {
            if (ps.planar)
            {
                // debug::write_dot_network(deleted_buffer_ntk, "deleted_buffer_ntk");
                deleted_buffer_ntk = insert_buffers_planar(deleted_buffer_ntk, old2new);
            }
            /*else
            {
                network_balancing_params b_ps{};
                if (ps.unify_outputs)
                {
                    b_ps.unify_outputs = true;
                }
                network_balancing<technology_network>(substituted, b_ps);
            }*/
        }

        return deleted_buffer_ntk;
    }

  private:
    /**
     * Parameters controlling how fanout substitution is performed.
     */
    const remove_buffer_params ps;
    /*
     * Input network.
     */
    Ntk ntk;
};

}  // namespace detail

/**
 * Deletes buffer in a network.
 *
 * @tparam Ntk Type of the input logic network.
 * @param ntk_src The input logic network.
 * @param ps Parameters.
 * @return A fanout-substituted logic network of type `NtkDest` that is logically equivalent to `ntk_src`.
 */
template <typename Ntk>
Ntk remove_buffer(const Ntk& ntk_src, remove_buffer_params ps = {})
{
    static_assert(mockturtle::is_network_type_v<Ntk>, "NtkSrc is not a network type");

    static_assert(mockturtle::has_is_constant_v<Ntk>, "NtkSrc does not implement the is_constant function");
    static_assert(mockturtle::has_create_pi_v<Ntk>, "NtkDest does not implement the create_pi function");
    static_assert(mockturtle::has_create_not_v<Ntk>, "NtkDest does not implement the create_not function");
    static_assert(mockturtle::has_create_po_v<Ntk>, "NtkDest does not implement the create_po function");
    static_assert(mockturtle::has_create_buf_v<Ntk>, "NtkDest does not implement the create_buf function");
    static_assert(mockturtle::has_clone_node_v<Ntk>, "NtkDest does not implement the clone_node function");
    static_assert(mockturtle::has_fanout_size_v<Ntk>, "NtkDest does not implement the fanout_size function");
    static_assert(mockturtle::has_foreach_gate_v<Ntk>, "NtkDest does not implement the foreach_gate function");
    static_assert(mockturtle::has_foreach_fanin_v<Ntk>, "NtkDest does not implement the foreach_fanin function");
    static_assert(mockturtle::has_foreach_po_v<Ntk>, "NtkDest does not implement the foreach_po function");

    detail::remove_buffer_impl<Ntk> p{ntk_src, ps};

    auto result = p.run();

    return result;
}

}  // namespace fiction

#endif  // FICTION_REMOVE_BUFFER_HPP
