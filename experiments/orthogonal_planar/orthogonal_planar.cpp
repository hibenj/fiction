//
// Created by benjamin on 20.08.24.
//

#include "fiction/algorithms/physical_design/orthogonal_planar.hpp"

#include "fiction/algorithms/network_transformation/network_balancing.hpp"
#include "fiction/algorithms/network_transformation/node_duplication_planarization.hpp"
#include "fiction/algorithms/physical_design/graph_oriented_layout_design.hpp"  // graph-oriented layout design algorithm
#include "fiction/algorithms/physical_design/orthogonal.hpp"  // graph-oriented layout design algorithm
#include "fiction/algorithms/verification/design_rule_violations.hpp"
#include "fiction/algorithms/verification/equivalence_checking.hpp"  // SAT-based equivalence checking
#include "fiction/algorithms/verification/virtual_miter.hpp"
#include "fiction/io/network_reader.hpp"     // read networks from files
#include "fiction/io/print_layout.hpp"
#include "fiction/layouts/bounding_box.hpp"  // calculate area of generated layouts
#include "fiction/layouts/clocked_layout.hpp"
#include "fiction/layouts/gate_level_layout.hpp"
#include "fiction/networks/technology_network.hpp"
#include "fiction/networks/views/extended_rank_view.hpp"
#include "fiction/utils/debug/network_writer.hpp"
#include "fiction_experiments.hpp"

#include <fmt/core.h>
#include <mockturtle/utils/stopwatch.hpp>
#include <mockturtle/views/rank_view.hpp>

#include <cstdlib>
#include <ostream>
#include <sstream>
#include <string>

template <typename Ntk>
Ntk read_ntk(const std::string& name)
{
    fmt::print("[i] processing {}\n", name);

    std::ostringstream                        os{};
    fiction::network_reader<fiction::tec_ptr> reader{fiction_experiments::benchmark_path(name), os};
    const auto                                nets    = reader.get_networks();
    const auto                                network = *nets.front();

    return network;
}

int main()  // NOLINT
{
    using gate_lyt =
        fiction::gate_level_layout<fiction::clocked_layout<fiction::tile_based_layout<fiction::cartesian_layout<>>>>;

    experiments::experiment<std::string, uint32_t, uint32_t, uint32_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t,
                            uint64_t, double, double, float, bool, std::string, std::string>
        orthogonal_planar_exp{"orthogonal_planar_exp",
                              "benchmark",
                              "inputs",
                              "outputs",
                              "initial nodes",
                              "layout width",
                              "layout height",
                              "optimized layout width",
                              "optimized layout height",
                              "layout area",
                              "optimized layout area",
                              "runtime planar [s]",
                              "runtime optimization [s]",
                              "improvement (%)",
                              "eq (ntks)",
                              "eq (ortho_p)",
                              "eq (post)"};

    fiction::orthogonal_physical_design_stats orthogonal_planar_stats{};
    fiction::post_layout_optimization_stats   post_layout_optimization_stats{};
    fiction::post_layout_optimization_params  params{};
    params.planar_optimization = true;
    fiction::graph_oriented_layout_design_params graph_oriented_layout_design_params{};
    graph_oriented_layout_design_params.high_effort_mode = true;
    graph_oriented_layout_design_params.verbose          = true;
    graph_oriented_layout_design_params.return_first     = true;
    graph_oriented_layout_design_params.timeout          = 1000000;
    graph_oriented_layout_design_params.planar           = true;

    static constexpr const uint64_t bench_select = (fiction_experiments::one_bit_add_maj);

    for (const auto& benchmark : fiction_experiments::all_benchmarks(bench_select))
    {
        auto                              net = read_ntk<fiction::tec_nt>(benchmark);
        fiction::network_balancing_params b_ps;
        b_ps.unify_outputs = true;

        const auto tec_b = fiction::network_balancing<fiction::technology_network>(
            fiction::fanout_substitution<fiction::technology_network>(net), b_ps);

        auto network = fiction::node_duplication_planarization<fiction::technology_network>(tec_b);
        fiction::debug::write_dot_network(network);

        mockturtle::equivalence_checking_stats eq_st;
        const auto                             cec_m = mockturtle::equivalence_checking(
            *fiction::virtual_miter<fiction::technology_network>(net, network), {}, &eq_st);
        assert(cec_m.has_value());

        const auto gate_level_layout = fiction::orthogonal_planar<gate_lyt>(network, {}, &orthogonal_planar_stats);

        fiction::debug::write_dot_network(network, "ntk_b");
        fiction::debug::write_dot_layout(gate_level_layout);

        // Ortho part
        /*fiction::orthogonal_physical_design_stats stats{};

        auto layout = fiction::orthogonal<gate_lyt>(network, {}, &stats);

        fiction::equivalence_checking_stats eq_s_ortho{};
        // check equivalence
        const auto eq_stats_ortho =
            fiction::equivalence_checking<decltype(network), gate_lyt>(network, *gate_level_layout, &eq_s_ortho);

        const std::string eq_result_ortho = eq_stats_ortho == fiction::eq_type::STRONG ? "STRONG" :
                                            eq_stats_ortho == fiction::eq_type::WEAK   ? "WEAK" :
                                                                                         "NO";*/

        fiction::equivalence_checking_stats eq_s{};
        // check equivalence
        const auto eq_stats =
            fiction::equivalence_checking<decltype(network), gate_lyt>(network, gate_level_layout, &eq_s);

        // Counter example
        std::cout << eq_s.impl_drv_stats.report;
        std::cout << "\n";
        std::cout << "Counter example: ";
        for (auto i : eq_s.counter_example) std::cout << i;
        std::cout << "\n";

        const std::string eq_result = eq_stats == fiction::eq_type::STRONG ? "STRONG" :
                                      eq_stats == fiction::eq_type::WEAK   ? "WEAK" :
                                                                             "NO";

        // calculate bounding box
        const auto bounding_box = fiction::bounding_box_2d(gate_level_layout);

        const auto width  = bounding_box.get_x_size() + 1;
        const auto height = bounding_box.get_y_size() + 1;
        const auto area   = width * height;

        fiction::gate_level_drv_params ps{};
        fiction::gate_level_drv_stats  st{};

        fiction::gate_level_drvs(gate_level_layout, ps, &st);
        // fiction::print_gate_level_layout(std::cout, gate_level_layout);

        const auto layout_copy = gate_level_layout.clone();
        fiction::post_layout_optimization(gate_level_layout, params, &post_layout_optimization_stats);

        // calculate bounding box
        const auto bounding_box_after_optimization = fiction::bounding_box_2d(gate_level_layout);

        const auto width_after_optimization  = bounding_box_after_optimization.get_x_size() + 1;
        const auto height_after_optimization = bounding_box_after_optimization.get_y_size() + 1;
        const auto area_after_optimization   = width_after_optimization * height_after_optimization;

        const float improv = 100 * static_cast<float>((area - area_after_optimization)) / static_cast<float>(area);

        // check equivalence
        const auto eq_stats_plo = fiction::equivalence_checking<gate_lyt, gate_lyt>(layout_copy, gate_level_layout);

        const std::string eq_result_plo = eq_stats_plo == fiction::eq_type::STRONG ? "STRONG" :
                                          eq_stats_plo == fiction::eq_type::WEAK   ? "WEAK" :
                                                                                     "NO";

        // log results
        orthogonal_planar_exp(benchmark, network.num_pis(), network.num_pos(), network.num_gates(), width, height,
                              width_after_optimization, height_after_optimization, area, area_after_optimization,
                              mockturtle::to_seconds(orthogonal_planar_stats.time_total),
                              mockturtle::to_seconds(post_layout_optimization_stats.time_total), improv,
                              cec_m.value(), eq_result, eq_result_plo);

        orthogonal_planar_exp.save();
        orthogonal_planar_exp.table();
    }

    return EXIT_SUCCESS;
}