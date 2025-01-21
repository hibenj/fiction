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
#include <cstdint>
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
int compute_pr_variables(const Ntk& ntk, const Lyt& lyt, uint32_t lvl)
{
    std::vector<uint8_t> orientation(ntk.rank_width(lvl));
    ntk.foreach_node_in_rank(lvl,
                             [&ntk, &orientation](const auto& n, const auto& i)
                             {
                                 if (ntk.fanin_size(n) == 2)
                                 {

                                 }
                                 if (const auto fc = fanins(ntk, n); fc.fanin_nodes.size() == 1)
                                 {
                                     const auto& pre = fc.fanin_nodes[0];
                                     if (ntk.is_fanout(pre) && ntk.fanout_size(pre) == 2)
                                     {
                                         std::fill(orientation.begin(), orientation.begin() + 3, 1);
                                     }
                                 }
                             });
    return 1;
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

        uint32_t lvl_test = 0;
        compute_pr_variables(ntk, layout, lvl_test);

        tile<Lyt> prec_pos{0, 0};
        tile<Lyt> first_pos = {ntk.num_pis(), 0};

        for (uint32_t lvl = 0; lvl < ntk.depth() + 1; lvl++)
        {

        }

        // layout.resize({first_pos.x + 1, prec_pos.y + 1, 0});

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

    orthogonal_physical_design_stats         st{};
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
