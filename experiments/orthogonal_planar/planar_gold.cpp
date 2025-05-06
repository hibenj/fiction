//
// Created by benjamin on 20.08.24.
//

#include "fiction/algorithms/graph/mincross.hpp"
#include "fiction/algorithms/network_transformation/buffer_removal.hpp"
#include "fiction/algorithms/network_transformation/network_balancing.hpp"
#include "fiction/algorithms/network_transformation/node_duplication_planarization.hpp"
#include "fiction/algorithms/physical_design/graph_oriented_layout_design.hpp"  // graph-oriented layout design algorithm
#include "fiction/algorithms/physical_design/orthogonal.hpp"  // graph-oriented layout design algorithm
#include "fiction/algorithms/physical_design/orthogonal_planar.hpp"
#include "fiction/algorithms/properties/critical_path_length_and_throughput.hpp"  // critical path and throughput calculations
#include "fiction/algorithms/verification/design_rule_violations.hpp"
#include "fiction/algorithms/verification/equivalence_checking.hpp"  // SAT-based equivalence checking
#include "fiction/algorithms/verification/virtual_miter.hpp"
#include "fiction/io/network_reader.hpp"     // read networks from files
#include "fiction/layouts/bounding_box.hpp"  // calculate area of generated layouts
#include "fiction/networks/technology_network.hpp"
#include "fiction/networks/views/mutable_rank_view.hpp"
#include "fiction/networks/virtual_pi_network.hpp"
#include "fiction/utils/debug/network_writer.hpp"
#include "fiction_experiments.hpp"

#include <fmt/format.h>  // output formatting
#include <mockturtle/views/rank_view.hpp>

#include <cstdlib>
#include <ostream>
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
                            uint64_t, double, double, float, std::string, std::string>
        graph_oriented_layout_design_exp{"graph_oriented_layout_design_exp",
                                         "benchmark",
                                         "inputs",
                                         "outputs",
                                         "initial nodes",
                                         "layout width (in tiles)",
                                         "layout height (in tiles)",
                                         "optimized layout width (in tiles)",
                                         "optimized layout height (in tiles)",
                                         "layout area (in tiles)",
                                         "optimized layout area (in tiles)",
                                         "runtime gold (in sec)",
                                         "runtime optimization (in sec)",
                                         "improvement (%)",
                                         "eq (gold)",
                                         "eq (plo)"};

    fiction::graph_oriented_layout_design_stats graph_oriented_layout_design_stats{};
    fiction::post_layout_optimization_stats     post_layout_optimization_stats{};
    fiction::post_layout_optimization_params    params{};
    params.planar_optimization = true;
    fiction::graph_oriented_layout_design_params graph_oriented_layout_design_params{};
    graph_oriented_layout_design_params.mode = fiction::graph_oriented_layout_design_params::effort_mode::HIGH_EFFORT;
    graph_oriented_layout_design_params.verbose      = true;
    graph_oriented_layout_design_params.return_first = false;
    graph_oriented_layout_design_params.timeout      = 100000;
    graph_oriented_layout_design_params.planar       = true;

    static constexpr const uint64_t bench_select = (fiction_experiments::full_adder);

    for (const auto& entry :
         std::filesystem::directory_iterator("/home/benjamin/Documents/Repositories/working/fiction/benchmarks/IWLS93"))
    {
        continue;
        fmt::print("[i] processing {}\n", entry.path().filename().string());

        if ("C432.v" == entry.path().filename().string())
        {
            continue;
        }
        if ("ex4p.v" == entry.path().filename().string())
        {
            continue;
        }
        if ("apex1.v" == entry.path().filename().string())
        {
            continue;
        }

        std::ostringstream os{};

        fiction::network_reader<fiction::tec_ptr> reader{entry.path().string(), os};

        const auto nets = reader.get_networks();

        auto net = *nets.front();

        bool cont = false;
        net.foreach_pi(
            [&net, &cont](auto pi)
            {
                if (net.is_po(pi))
                {
                    cont = true;
                }
            });
        if (cont)
        {
            continue;
        }

        fiction::network_balancing_params ps;
        ps.unify_outputs = true;

        const auto tec_b = fiction::network_balancing<fiction::technology_network>(
            fiction::fanout_substitution<fiction::technology_network>(net), ps);

        if (tec_b.size() > 10000)
        {
            continue;
        }

        const auto tec_r   = fiction::mutable_rank_view(tec_b);
        auto       network = fiction::node_duplication_planarization(tec_r);

        /*if (network.num_gates() > 1000)
        {
            continue;
        }*/

        const auto planar = network.clone();
        const auto ntk    = fiction::remove_buffer(planar);

        if (ntk.num_gates() > 1000)
        {
            continue;
        }

        /*mockturtle::equivalence_checking_stats eq_st;
        const auto                             cec_m = mockturtle::equivalence_checking(
            *fiction::virtual_miter<fiction::technology_network>(network, ntk), {}, &eq_st);
        assert(cec_m.has_value());
        std::cout << "Ist Equivalent:" << cec_m.value() << std::endl;*/

        auto gate_level_layout = fiction::graph_oriented_layout_design<gate_lyt, decltype(ntk)>(
            network, graph_oriented_layout_design_params, &graph_oriented_layout_design_stats);

        if (gate_level_layout.has_value())
        {
            fiction::equivalence_checking_stats eq_s{};
            // check equivalence
            const auto eq_stats =
                fiction::equivalence_checking<decltype(ntk), gate_lyt>(ntk, *gate_level_layout, &eq_s);

            /* std::cout << eq_s.impl_drv_stats.report;
             std::cout << "\n";
             std::cout << "Counter example: ";
             for (auto i : eq_s.counter_example)
                 std::cout << i;
             std::cout << "\n";*/

            const std::string eq_result = eq_stats == fiction::eq_type::STRONG ? "STRONG" :
                                          eq_stats == fiction::eq_type::WEAK   ? "WEAK" :
                                                                                 "NO";

            // calculate bounding box
            const auto bounding_box = fiction::bounding_box_2d(*gate_level_layout);

            const auto width  = bounding_box.get_x_size() + 1;
            const auto height = bounding_box.get_y_size() + 1;
            const auto area   = width * height;

            fiction::gate_level_drv_params ps{};
            fiction::gate_level_drv_stats  st{};

            fiction::gate_level_drvs(*gate_level_layout, ps, &st);
            // fiction::print_gate_level_layout(std::cout, *gate_level_layout);

            const auto layout_copy = gate_level_layout->clone();
            fiction::post_layout_optimization(*gate_level_layout, params, &post_layout_optimization_stats);

            // calculate bounding box
            const auto bounding_box_after_optimization = fiction::bounding_box_2d(*gate_level_layout);

            const auto width_after_optimization  = bounding_box_after_optimization.get_x_size() + 1;
            const auto height_after_optimization = bounding_box_after_optimization.get_y_size() + 1;
            const auto area_after_optimization   = width_after_optimization * height_after_optimization;

            const float improv = 100 * static_cast<float>((area - area_after_optimization)) / static_cast<float>(area);

            // check equivalence
            const auto eq_stats_plo =
                fiction::equivalence_checking<gate_lyt, gate_lyt>(layout_copy, *gate_level_layout);

            const std::string eq_result_plo = eq_stats_plo == fiction::eq_type::STRONG ? "STRONG" :
                                              eq_stats_plo == fiction::eq_type::WEAK   ? "WEAK" :
                                                                                         "NO";

            // log results
            graph_oriented_layout_design_exp(
                entry.path().filename().string(), ntk.num_pis(), ntk.num_pos(), ntk.num_gates(), width, height,
                width_after_optimization, height_after_optimization, area, area_after_optimization,
                mockturtle::to_seconds(graph_oriented_layout_design_stats.time_total),
                mockturtle::to_seconds(post_layout_optimization_stats.time_total), improv, eq_result, eq_result_plo);
        }
        else
        {
            // log results
            graph_oriented_layout_design_exp(
                entry.path().filename().string(), ntk.num_pis(), ntk.num_pos(), ntk.num_gates(), 0, 0, 0, 0, 0, 0,
                mockturtle::to_seconds(graph_oriented_layout_design_stats.time_total),
                mockturtle::to_seconds(post_layout_optimization_stats.time_total), 0, "?", "?");
        }

        graph_oriented_layout_design_exp.save();
        graph_oriented_layout_design_exp.table();
    }

    for (const auto& benchmark : fiction_experiments::all_benchmarks(bench_select))
    {
        // continue;

        fiction::technology_network tec{};
        const auto                  pi0 = tec.create_pi();  // Cin
        const auto                  pi1 = tec.create_pi();  // A
        const auto                  pi2 = tec.create_pi();  // B
        // level 1
        const auto s10 = tec.create_buf(pi0);
        const auto s11 = tec.create_buf(pi1);
        const auto s12 = tec.create_buf(pi2);
        // level2
        const auto s20 = tec.create_buf(s10);
        const auto s21 = tec.create_xor(s11, s12);
        const auto s22 = tec.create_buf(s12);
        // level3
        const auto s30 = tec.create_buf(s20);
        const auto s31 = tec.create_buf(s21);
        const auto s32 = tec.create_buf(s22);
        // level4
        const auto s40 = tec.create_xor(s30, s31);
        const auto s41 = tec.create_buf(s31);
        const auto s42 = tec.create_buf(s32);
        // level4 out
        const auto s4x  = tec.create_buf(s40);
        const auto s40x = tec.create_buf(s41);
        const auto s41x = tec.create_buf(s41);
        const auto s42x = tec.create_buf(s42);
        // level5
        const auto s5  = tec.create_buf(s4x);
        const auto s50 = tec.create_xor(s4x, s40x);
        const auto s51 = tec.create_buf(s41x);
        const auto s52 = tec.create_buf(s42x);
        // level6
        const auto s6  = tec.create_buf(s5);
        const auto s60 = tec.create_and(s50, s51);
        const auto s61 = tec.create_xor(s51, s52);
        const auto s62 = tec.create_buf(s52);
        // level7
        const auto s7  = tec.create_buf(s6);
        const auto s70 = tec.create_buf(s60);
        const auto s71 = tec.create_and(s61, s62);
        // level8
        const auto s8  = tec.create_buf(s7);
        const auto s80 = tec.create_or(s70, s71);

        tec.create_po(s8);
        tec.create_po(s80);

        auto       net = read_ntk<fiction::tec_nt>(benchmark);
        const auto eq =
            fiction::equivalence_checking<fiction::technology_network, fiction::technology_network>(net, tec);
        const std::string eq_str = eq == fiction::eq_type::STRONG ? "STRONG" :
                                   eq == fiction::eq_type::WEAK   ? "WEAK" :
                                                                    "NO";

        std::cout << "Equivalence is " << eq_str << std::endl;
        fiction::network_balancing_params ps;
        ps.unify_outputs = true;
        // fiction::debug::write_dot_network(net);

        const auto tec_b = fiction::network_balancing<fiction::technology_network>(
            fiction::fanout_substitution<fiction::technology_network>(net), ps);

        const auto tec_r = fiction::mutable_rank_view(tec_b);
        auto       ntk   = fiction::node_duplication_planarization(tec_r);

        fiction::mincross_stats        st{};
        const fiction::mincross_params p{};
        auto                           ntk_mincross = mincross(tec_r, p, &st);
        std::cout << "Num crossings: " << st.num_crossings << std::endl;
        fiction::debug::write_dot_network(ntk_mincross, "mincross");

        const auto planar  = ntk.clone();
        const auto network = fiction::remove_buffer(planar);
        fiction::debug::write_dot_network(network, "planar");

        auto gate_level_layout = fiction::graph_oriented_layout_design<gate_lyt, decltype(network)>(
            network, graph_oriented_layout_design_params, &graph_oriented_layout_design_stats);

        if (gate_level_layout.has_value())
        {
            fiction::equivalence_checking_stats eq_s{};
            // check equivalence
            const auto eq_stats =
                fiction::equivalence_checking<decltype(network), gate_lyt>(network, *gate_level_layout, &eq_s);

            /* std::cout << eq_s.impl_drv_stats.report;
             std::cout << "\n";
             std::cout << "Counter example: ";
             for (auto i : eq_s.counter_example)
                 std::cout << i;
             std::cout << "\n";*/

            const std::string eq_result = eq_stats == fiction::eq_type::STRONG ? "STRONG" :
                                          eq_stats == fiction::eq_type::WEAK   ? "WEAK" :
                                                                                 "NO";

            // calculate bounding box
            const auto bounding_box = fiction::bounding_box_2d(*gate_level_layout);

            const auto width  = bounding_box.get_x_size() + 1;
            const auto height = bounding_box.get_y_size() + 1;
            const auto area   = width * height;

            fiction::gate_level_drv_params ps{};
            fiction::gate_level_drv_stats  st{};

            fiction::gate_level_drvs(*gate_level_layout, ps, &st);
            // fiction::print_gate_level_layout(std::cout, *gate_level_layout);

            const auto layout_copy = gate_level_layout->clone();
            fiction::post_layout_optimization(*gate_level_layout, params, &post_layout_optimization_stats);
            fiction::debug::write_dot_layout(*gate_level_layout);

            // calculate bounding box
            const auto bounding_box_after_optimization = fiction::bounding_box_2d(*gate_level_layout);

            const auto width_after_optimization  = bounding_box_after_optimization.get_x_size() + 1;
            const auto height_after_optimization = bounding_box_after_optimization.get_y_size() + 1;
            const auto area_after_optimization   = width_after_optimization * height_after_optimization;

            const float improv = 100 * static_cast<float>((area - area_after_optimization)) / static_cast<float>(area);

            // check equivalence
            const auto eq_stats_plo =
                fiction::equivalence_checking<gate_lyt, gate_lyt>(layout_copy, *gate_level_layout);

            const std::string eq_result_plo = eq_stats_plo == fiction::eq_type::STRONG ? "STRONG" :
                                              eq_stats_plo == fiction::eq_type::WEAK   ? "WEAK" :
                                                                                         "NO";

            // log results
            graph_oriented_layout_design_exp(
                benchmark, network.num_pis(), network.num_pos(), network.num_gates(), width, height,
                width_after_optimization, height_after_optimization, area, area_after_optimization,
                mockturtle::to_seconds(graph_oriented_layout_design_stats.time_total),
                mockturtle::to_seconds(post_layout_optimization_stats.time_total), improv, eq_result, eq_result_plo);
        }
        else
        {
            // log results
            graph_oriented_layout_design_exp(
                benchmark, network.num_pis(), network.num_pos(), network.num_gates(), 0, 0, 0, 0, 0, 0,
                mockturtle::to_seconds(graph_oriented_layout_design_stats.time_total),
                mockturtle::to_seconds(post_layout_optimization_stats.time_total), 0, "?", "?");
        }

        graph_oriented_layout_design_exp.save();
        graph_oriented_layout_design_exp.table();
    }

    return EXIT_SUCCESS;
}