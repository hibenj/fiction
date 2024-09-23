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
#include "fiction/io/network_reader.hpp"  // read networks from files
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

    std::ostringstream os{};

    fiction::network_reader<fiction::tec_ptr> reader{fiction_experiments::benchmark_path(name), os};

    const auto nets = reader.get_networks();

    return *nets.front();
}

int main()  // NOLINT
{
    using gate_lyt =
        fiction::gate_level_layout<fiction::clocked_layout<fiction::tile_based_layout<fiction::cartesian_layout<>>>>;

    experiments::experiment<std::string, uint32_t, uint32_t, uint32_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t,
                            uint64_t, double, double, float, bool, bool, bool, bool>
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
                              "planar_layout",
                              "eq (ntks)",
                              "eq (ortho_p)",
                              "eq (post)"};

    fiction::orthogonal_physical_design_stats orthogonal_planar_stats{};
    fiction::post_layout_optimization_stats   post_layout_optimization_stats{};
    fiction::post_layout_optimization_params  params{};

    // For IWLS benchmarks
    int x = 0;
    for (const auto& entry :
         std::filesystem::directory_iterator("/home/benjamin/Documents/Repositories/working/fiction/benchmarks/IWLS93"))
    {
        continue;
        fmt::print("[i] processing {}\n", entry.path().filename().string());

        if ( "C432.v" == entry.path().filename().string())
        {
            continue;
        }
        if ( "ex4p.v" == entry.path().filename().string())
        {
            continue;
        }
        if ( "apex1.v" == entry.path().filename().string())
        {
            continue;
        }

        std::ostringstream os{};

        fiction::network_reader<fiction::tec_ptr> reader{entry.path().string(), os};

        const auto nets = reader.get_networks();

        auto benchmark_network = *nets.front();

        bool cont = false;
        benchmark_network.foreach_pi(
            [&benchmark_network, &cont](auto pi)
            {
                if (benchmark_network.is_po(pi))
                {
                    cont = true;
                }
            });
        if (cont)
        {
            continue;
        }

        fiction::network_balancing_params b_ps;
        b_ps.unify_outputs = true;

        const auto _b = fiction::network_balancing<fiction::technology_network>(
            fiction::fanout_substitution<fiction::technology_network>(benchmark_network), b_ps);

        if (_b.size() > 10000)
        {
            continue;
        }

        auto planarized_b = fiction::node_duplication_planarization<fiction::technology_network>(_b);

        if (planarized_b.size() > 20000)
        {
            continue;
        }

        // chek equivalence after planarization
        mockturtle::equivalence_checking_stats eq_st;
        const auto                             cec_m = mockturtle::equivalence_checking(
            *fiction::virtual_miter<fiction::technology_network>(benchmark_network, planarized_b), {}, &eq_st);
        assert(cec_m.has_value());

        const auto gate_level_layout = fiction::orthogonal_planar<gate_lyt>(planarized_b, {}, &orthogonal_planar_stats);

        bool planar_layout = false;
        if(gate_level_layout.num_crossings() == 0)
        {
            planar_layout = true;
        }

        gate_level_layout.num_crossings();

        fiction::gate_level_drv_params ps{};
        fiction::gate_level_drv_stats  st{};

        fiction::gate_level_drvs(gate_level_layout, ps, &st);

        // check equivalence for the planar layout
        const auto miter = mockturtle::miter<mockturtle::klut_network>(planarized_b, gate_level_layout);
        bool       eq;
        if (miter)
        {
            mockturtle::equivalence_checking_stats st;

            const auto ce = mockturtle::equivalence_checking(*miter, {}, &st);
            eq            = ce.value();
        }

        // Ortho part
        fiction::orthogonal_physical_design_stats stats{};

        auto layout = fiction::orthogonal<gate_lyt>(planarized_b, {}, &stats);

        const auto miter_o = mockturtle::miter<mockturtle::klut_network>(planarized_b, gate_level_layout);
        bool       eq_o;
        if (miter_o)
        {
            mockturtle::equivalence_checking_stats st;

            const auto ce = mockturtle::equivalence_checking(*miter_o, {}, &st);
            eq_o            = ce.value();
        }
        std::cout << "Eq: " << eq_o << '\n';

        orthogonal_planar_exp(
            entry.path().filename().string(), planarized_b.num_pis(), planarized_b.num_pos(), planarized_b.num_gates(), gate_level_layout.x(),
            gate_level_layout.y(), 0, 0, 0, 0, mockturtle::to_seconds(orthogonal_planar_stats.time_total),
            mockturtle::to_seconds(post_layout_optimization_stats.time_total), 0, planar_layout, cec_m.value(), eq, 0);

        orthogonal_planar_exp.save();
        orthogonal_planar_exp.table();

        std::cout << ++x << std::endl;
    }

    // For all fiction benchmarks
    static constexpr const uint64_t bench_select = (fiction_experiments::mux21);

    for (const auto& benchmark : fiction_experiments::all_benchmarks(bench_select))
    {
        // auto                              benchmark_network = read_ntk<fiction::tec_nt>(benchmark);

        fiction::technology_network benchmark_network;
        const auto pi0 = benchmark_network.create_pi();
        const auto pi1 = benchmark_network.create_pi();
        const auto pi2 = benchmark_network.create_pi();
        const auto pi3 = benchmark_network.create_pi();
        const auto pi4 = benchmark_network.create_pi();
        const auto pi5 = benchmark_network.create_pi();

        const auto n2 = benchmark_network.create_not(pi2);
        const auto n3 = benchmark_network.create_not(pi3);
        const auto a0 = benchmark_network.create_and(pi0, pi1);
        const auto a1 = benchmark_network.create_and(pi4, pi5);
        const auto a2 = benchmark_network.create_and(a0, pi2);
        const auto a3 = benchmark_network.create_and(n2, n3);
        const auto a4 = benchmark_network.create_and(a1, pi3);
        const auto o0 = benchmark_network.create_or(pi4, pi5);

        benchmark_network.create_po(a2);
        benchmark_network.create_po(a3);
        benchmark_network.create_po(a4);
        benchmark_network.create_po(o0);

        fiction::debug::write_dot_network(benchmark_network, "original_ntk");

        fiction::network_balancing_params b_ps;
        b_ps.unify_outputs = true;

        bool cont = false;
        benchmark_network.foreach_pi(
            [&benchmark_network, &cont](auto pi)
            {
                if (benchmark_network.is_po(pi))
                {
                    cont = true;
                    // std::cout << "Pi is Po\n";
                }
            });
        if (cont)
        {
            continue;
        }

        const auto _b = fiction::network_balancing<fiction::technology_network>(
            fiction::fanout_substitution<fiction::technology_network>(benchmark_network), b_ps);

        fiction::debug::write_dot_network(_b, "balanced_ntk");

        if (_b.size() > 10000)
        {
            continue;
        }

        auto planarized_b = fiction::node_duplication_planarization<fiction::technology_network>(_b);

        fiction::debug::write_dot_network(planarized_b, "planarized_ntk");

        if (planarized_b.size() > 100000)
        {
            continue;
        }

        // chek equivalence after planarization
        mockturtle::equivalence_checking_stats eq_st;
        const auto                             cec_m = mockturtle::equivalence_checking(
            *fiction::virtual_miter<fiction::technology_network>(benchmark_network, planarized_b), {}, &eq_st);
        assert(cec_m.has_value());

        const auto gate_level_layout = fiction::orthogonal_planar<gate_lyt>(planarized_b, {}, &orthogonal_planar_stats);

        bool planar_layout = false;
        if(gate_level_layout.num_crossings() == 0)
        {
            planar_layout = true;
        }

        fiction::gate_level_drv_params ps_p{};
        fiction::gate_level_drv_stats  st_p{};

        fiction::gate_level_drvs(gate_level_layout, ps_p, &st_p);

        // check equivalence for the planar layout
        const auto miter = mockturtle::miter<mockturtle::klut_network>(planarized_b, gate_level_layout);
        bool       eq = false;
        if (miter)
        {
            mockturtle::equivalence_checking_stats st;

            const auto ce = mockturtle::equivalence_checking(*miter, {}, &st);
            eq            = ce.value();
        }

        /*fiction::debug::write_dot_network(network, "ntk_b");
        fiction::debug::write_dot_layout(gate_level_layout);*/
        fiction::debug::write_dot_layout(gate_level_layout, "planarized_lyt");

        // Ortho part
        fiction::orthogonal_physical_design_stats stats{};

        auto layout = fiction::orthogonal<gate_lyt>(planarized_b, {}, &stats);

        const auto miter_o = mockturtle::miter<mockturtle::klut_network>(planarized_b, gate_level_layout);
        bool       eq_o = false;
        if (miter_o)
        {
            mockturtle::equivalence_checking_stats st;

            const auto ce = mockturtle::equivalence_checking(*miter_o, {}, &st);
            eq_o            = ce.value();
        }
        std::cout << "Eq: " << eq_o << '\n';

        // Counter example
        /*std::cout << eq_s.impl_drv_stats.report;
        std::cout << "\n";
        std::cout << "Counter example: ";
        for (auto i : eq_s.counter_example) std::cout << i;
        std::cout << "\n";*/

        // calculate bounding box
        const auto bounding_box = fiction::bounding_box_2d(gate_level_layout);

        const auto width  = bounding_box.get_x_size() + 1;
        const auto height = bounding_box.get_y_size() + 1;
        const auto area   = width * height;

        fiction::gate_level_drv_params ps_post{};
        fiction::gate_level_drv_stats  st_post{};

        fiction::gate_level_drvs(gate_level_layout, ps_post, &st_post);
        // fiction::print_gate_level_layout(std::cout, gate_level_layout);

        // const auto layout_copy = gate_level_layout.clone();
        fiction::post_layout_optimization(gate_level_layout, params, &post_layout_optimization_stats);

        fiction::debug::write_dot_layout(gate_level_layout, "optimized_lyt");

        // calculate bounding box
        const auto bounding_box_after_optimization = fiction::bounding_box_2d(gate_level_layout);

        const auto width_after_optimization  = bounding_box_after_optimization.get_x_size() + 1;
        const auto height_after_optimization = bounding_box_after_optimization.get_y_size() + 1;
        const auto area_after_optimization   = width_after_optimization * height_after_optimization;

        const float improv = 100 * static_cast<float>((area - area_after_optimization)) / static_cast<float>(area);

        // check equivalence
        const auto miter_post = mockturtle::miter<mockturtle::klut_network>(planarized_b, gate_level_layout);
        bool eq_post;
        if (miter_post)
        {
            mockturtle::equivalence_checking_stats st_post;

            const auto ce = mockturtle::equivalence_checking(*miter_post, {}, &st_post);
            eq_post = ce.value();
        }

        // log results
        orthogonal_planar_exp(benchmark, planarized_b.num_pis(), planarized_b.num_pos(), planarized_b.num_gates(),
           width, height, width_after_optimization, height_after_optimization, area, area_after_optimization,
                              mockturtle::to_seconds(orthogonal_planar_stats.time_total),
                              mockturtle::to_seconds(post_layout_optimization_stats.time_total), improv, planar_layout,
                              cec_m.value(), eq, eq_post);

       /* orthogonal_planar_exp(
            benchmark, planarized_b.num_pis(), planarized_b.num_pos(), planarized_b.num_gates(), gate_level_layout.x(),
            gate_level_layout.y(), 0, 0, 0, 0, mockturtle::to_seconds(orthogonal_planar_stats.time_total),
            mockturtle::to_seconds(post_layout_optimization_stats.time_total), 0, planar_layout, cec_m.value(), eq, 0);*/

        orthogonal_planar_exp.save();
        orthogonal_planar_exp.table();
    }

    return EXIT_SUCCESS;
}