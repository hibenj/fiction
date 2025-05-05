//
// Created by benjamin on 20.08.24.
//

#include "fiction/algorithms/physical_design/orthogonal_planar.hpp"

#include "fiction/algorithms/network_transformation/inverter_substitution.hpp"
#include "fiction/algorithms/network_transformation/network_balancing.hpp"
#include "fiction/algorithms/network_transformation/node_duplication_planarization.hpp"
#include "fiction/algorithms/physical_design/apply_gate_library.hpp"
#include "fiction/algorithms/physical_design/graph_oriented_layout_design.hpp"  // graph-oriented layout design algorithm
#include "fiction/algorithms/physical_design/orthogonal.hpp"  // graph-oriented layout design algorithm
#include "fiction/algorithms/physical_design/orthogonal_planar_v2.hpp"
#include "fiction/algorithms/verification/design_rule_violations.hpp"
#include "fiction/algorithms/verification/equivalence_checking.hpp"  // SAT-based equivalence checking
#include "fiction/algorithms/verification/virtual_miter.hpp"
#include "fiction/io/network_reader.hpp"  // read networks from files
#include "fiction/io/print_layout.hpp"
#include "fiction/io/write_qca_layout.hpp"
#include "fiction/io/write_svg_layout.hpp"
#include "fiction/layouts/bounding_box.hpp"  // calculate area of generated layouts
#include "fiction/layouts/clocked_layout.hpp"
#include "fiction/layouts/gate_level_layout.hpp"
#include "fiction/networks/technology_network.hpp"
#include "fiction/networks/views/mutable_rank_view.hpp"
#include "fiction/technology/qca_one_library.hpp"
#include "fiction/utils/debug/network_writer.hpp"
#include "fiction_experiments.hpp"

#include <fmt/core.h>
#include <mockturtle/utils/stopwatch.hpp>
#include <mockturtle/views/rank_view.hpp>

#include <cstdlib>
#include <ostream>
#include <sstream>
#include <string>

template <typename Ntk1, typename Ntk2>
inline bool abc_cec_two_ntk(Ntk1 const& ntk1, Ntk2 const& ntk2)
{
    mockturtle::write_bench(ntk1, "/tmp/test1.bench");
    mockturtle::write_bench(ntk2, "/tmp/test2.bench");
    std::string command = fmt::format("abc -q \"cec -n /tmp/test1.bench /tmp/test2.bench\"");

    std::array<char, 128> buffer;
    std::string           result;
#if WIN32
    std::unique_ptr<FILE, decltype(&_pclose)> pipe(_popen(command.c_str(), "r"), _pclose);
#else
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command.c_str(), "r"), pclose);
#endif
    if (!pipe)
    {
        throw std::runtime_error("popen() failed");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr)
    {
        result += buffer.data();
    }

    /* search for one line which says "Networks are equivalent" and ignore all other debug output from ABC */
    std::stringstream ss(result);
    std::string       line;
    while (std::getline(ss, line, '\n'))
    {
        if (line.size() >= 23u && line.substr(0u, 23u) == "Networks are equivalent")
        {
            return true;
        }
    }

    return false;
}

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
    using qca_cell_level_layout =
        fiction::cell_level_layout<fiction::qca_technology,
                                   fiction::clocked_layout<fiction::cartesian_layout<fiction::offset::ucoord_t>>>;

    experiments::experiment<std::string, uint32_t, uint32_t, uint32_t, uint64_t, uint64_t, uint64_t, double, uint64_t, uint64_t,
                            uint64_t, double, double, double, bool, bool, bool>
        orthogonal_planar_exp{"orthogonal_planar_exp",
                              "benchmark",
                              "inputs",
                              "outputs",
                              "initial nodes",
                              "layout width sota",
                              "layout height sota",
                              "layout area sota",
                              "runtime sota",
                              "layout width proposed",
                              "layout height proposed",
                              "layout area proposed",
                              "runtime proposed",
                              "diff area (%)",
                              "diff runtime (%)",
                              "planar_layout",
                              "eq (ntks)",
                              "eq (ortho_p)"};

    fiction::orthogonal_physical_design_stats orthogonal_planar_stats{};
    fiction::orthogonal_physical_design_stats orthogonal_planar_stats_sota{};
    fiction::post_layout_optimization_params  params{};
    params.max_gate_relocations = 0;
    params.timeout              = 100000;

    // For IWLS benchmarks
    int x = 0;
    for (const auto& entry :
         std::filesystem::directory_iterator("/home/benjamin/Documents/Repositories/working/fiction/benchmarks/IWLS93"))
    {
        // continue;
        fmt::print("[i] processing {}\n", entry.path().filename().string());

        /*if ("C432.v" == entry.path().filename().string())
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
        if ("term1.v" == entry.path().filename().string())
        {
            continue;
        }*/
        /*if ("C880.v" != entry.path().filename().string())
        {
            continue;
        }*/

        if (!("x4.v" == entry.path().filename().string() || "duke2.v" == entry.path().filename().string() || "rd84.v" == entry.path().filename().string() ||
              "t481.v" == entry.path().filename().string() || "C880.v" == entry.path().filename().string() || "vda.v" == entry.path().filename().string() ||
              "table5.v" == entry.path().filename().string() || "table3.v" == entry.path().filename().string() || "apex3.v" == entry.path().filename().string() ||
              "cordic.v" == entry.path().filename().string()))
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

        /*if (_b.size() > 1000)
        {
            continue;
        }*/

        auto _r = fiction::mutable_rank_view(_b);
        auto planarized_b = fiction::node_duplication_planarization(_r);

        std::cout << "planarization finished\n";

        if (planarized_b.num_gates() > 150000)
        {
            continue;
        }

        // chek equivalence after planarization
        mockturtle::equivalence_checking_stats eq_st;
        const auto                             cec_m = mockturtle::equivalence_checking(
            *fiction::virtual_miter<fiction::technology_network>(benchmark_network, planarized_b), {}, &eq_st);
        assert(cec_m.has_value());

        const auto gate_level_layout_sota = fiction::orthogonal_planar<gate_lyt>(planarized_b, {}, &orthogonal_planar_stats_sota);
        const auto gate_level_layout_proposed = fiction::orthogonal_planar_v2<gate_lyt>(planarized_b, {}, &orthogonal_planar_stats);

        std::cout << "layout finished\n";

        // fiction::debug::write_dot_network(planarized_b);
        // fiction::debug::write_dot_layout(gate_level_layout);

        bool planar_layout = false;
        if (gate_level_layout_proposed.num_crossings() == 0)
        {
            planar_layout = true;
        }

        fiction::gate_level_drv_params ps_p{};
        fiction::gate_level_drv_stats  st_p{};

        fiction::gate_level_drvs(gate_level_layout_proposed, ps_p, &st_p);

        // check equivalence for the planar layout
        const auto miter = mockturtle::miter<mockturtle::klut_network>(planarized_b, gate_level_layout_proposed);
        bool       eq = false;
        if (miter)
        {
            mockturtle::equivalence_checking_stats st;

            const auto ce = mockturtle::equivalence_checking(*miter, {}, &st);
            eq            = ce.value();
        }

        std::cout << "equivalence finished\n";

        // Ortho part
        fiction::orthogonal_physical_design_stats stats{};

        // calculate bounding box
        const auto bounding_box_sota = fiction::bounding_box_2d(gate_level_layout_sota);
        const auto width_sota  = bounding_box_sota.get_x_size() + 1;
        const auto height_sota = bounding_box_sota.get_y_size() + 1;
        const auto area_sota   = width_sota * height_sota;
        const auto time_sota   = mockturtle::to_seconds(orthogonal_planar_stats_sota.time_total);

        // calculate bounding box
        const auto bounding_box_proposed = fiction::bounding_box_2d(gate_level_layout_proposed);
        const auto width_proposed  = bounding_box_proposed.get_x_size() + 1;
        const auto height_proposed = bounding_box_proposed.get_y_size() + 1;
        const auto area_proposed   = width_proposed * height_proposed;
        const auto time_proposed   = mockturtle::to_seconds(orthogonal_planar_stats.time_total);

        const auto area_diff = (static_cast<double>(area_sota - area_proposed) / area_sota) * 100.0;
        const auto time_diff = (static_cast<double>(time_sota - time_proposed) / time_sota) * 100.0;

        orthogonal_planar_exp(entry.path().filename().string(), planarized_b.num_pis(), planarized_b.num_pos(),
                              planarized_b.num_gates(), width_sota, height_sota, area_sota, time_sota,
                              width_proposed, height_proposed, area_proposed, time_proposed,
                              area_diff,
                              time_diff, planar_layout,
                              cec_m.value(), eq);

        orthogonal_planar_exp.save();
        orthogonal_planar_exp.table();

        std::cout << ++x << std::endl;
    }

    // For all fiction benchmarks
    static constexpr const uint64_t bench_select = (fiction_experiments::trindade16 | fiction_experiments::fontes18);

    for (const auto& benchmark : fiction_experiments::all_benchmarks(bench_select))
    {
        continue;
        auto benchmark_network = read_ntk<fiction::tec_nt>(benchmark);

        /*fiction::technology_network benchmark_network;
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
        benchmark_network.create_po(o0);*/

        // fiction::debug::write_dot_network(benchmark_network, "original_ntk");

        fiction::network_balancing_params b_ps;
        b_ps.unify_outputs = true;

        /*bool cont = false;
        benchmark_network.foreach_pi(
            [&benchmark_network, &cont](auto pi)
            {
                if (benchmark_network.is_po(pi))
                {
                    cont = true;
                    std::cout << "Pi is Po\n";
                }
            });
        if (cont)
        {
            continue;
        }*/

        const auto _b = fiction::network_balancing<fiction::technology_network>(
            fiction::fanout_substitution<fiction::technology_network>(benchmark_network), b_ps);

        // fiction::debug::write_dot_network(_b, "balanced_ntk");

        if (_b.size() > 10000)
        {
            continue;
        }

        auto _r = fiction::mutable_rank_view(_b);
        auto planarized_b = fiction::node_duplication_planarization(_r);
        //const auto ortho = fiction::orthogonal<gate_lyt>(planarized_b);
        //const auto mapped_to_cells = fiction::apply_gate_library<fiction::qca_cell_clk_lyt, fiction::qca_one_library>(ortho);
        //fiction::debug::write_dot_network(planarized_b, "planarized_ntk");
        //fiction::debug::write_dot_layout(ortho, "ortho_lyt");
        //fiction::write_qca_layout_svg(mapped_to_cells, "ortho_cell_lyt");

        if (planarized_b.size() > 100000)
        {
            continue;
        }

        //fiction::detail::operation_mode mode          = fiction::detail::operation_mode::AND_OR_ONLY;
        //auto                            substituted_b = inverter_substitution(planarized_b, mode);

        //fiction::debug::write_dot_network(substituted_b, "substituted_ntk");

        // chek equivalence after planarization
        mockturtle::equivalence_checking_stats eq_st;
        const auto                             cec_m = mockturtle::equivalence_checking(
            *fiction::virtual_miter<fiction::technology_network>(benchmark_network, planarized_b), {}, &eq_st);
        assert(cec_m.has_value());

        const auto gate_level_layout_sota = fiction::orthogonal_planar<gate_lyt>(planarized_b, {}, &orthogonal_planar_stats);
        const auto gate_level_layout_proposed = fiction::orthogonal_planar_v2<gate_lyt>(planarized_b, {}, &orthogonal_planar_stats);
        //const auto mapped_to_cells_planar = fiction::apply_gate_library<fiction::qca_cell_clk_lyt, fiction::qca_one_library>(gate_level_layout);
        //fiction::write_qca_layout_svg(mapped_to_cells_planar, "planar_cell_lyt");

        bool planar_layout = false;
        if (gate_level_layout_proposed.num_crossings() == 0)
        {
            planar_layout = true;
        }

        fiction::gate_level_drv_params ps_p{};
        fiction::gate_level_drv_stats  st_p{};

        fiction::gate_level_drvs(gate_level_layout_proposed, ps_p, &st_p);

        // fiction::debug::write_dot_layout(gate_level_layout);

        // check equivalence for the planar layout
        const auto miter = mockturtle::miter<mockturtle::klut_network>(planarized_b, gate_level_layout_proposed);
        bool       eq    = false;
        if (miter)
        {
            mockturtle::equivalence_checking_stats st;

            const auto ce = mockturtle::equivalence_checking(*miter, {}, &st);
            eq            = ce.value();
        }

        // calculate bounding box
        const auto bounding_box = fiction::bounding_box_2d(gate_level_layout_proposed);

        const auto width  = bounding_box.get_x_size() + 1;
        const auto height = bounding_box.get_y_size() + 1;
        const auto area   = width * height;

        fiction::gate_level_drv_params ps_post{};
        fiction::gate_level_drv_stats  st_post{};

        fiction::gate_level_drvs(gate_level_layout_proposed, ps_post, &st_post);
        // fiction::print_gate_level_layout(std::cout, gate_level_layout);

        // const auto layout_copy = gate_level_layout.clone();
        // fiction::post_layout_optimization(gate_level_layout, params, &post_layout_optimization_stats);

        /*auto ortho_cell_layout_post =
            fiction::apply_gate_library<qca_cell_level_layout, fiction::qca_one_library>(gate_level_layout);
        fiction::write_qca_layout_svg(ortho_cell_layout_post, "optimized_cell_lyt");*/

        // calculate bounding box
        const auto bounding_box_sota = fiction::bounding_box_2d(gate_level_layout_sota);
        const auto width_sota  = bounding_box_sota.get_x_size() + 1;
        const auto height_sota = bounding_box_sota.get_y_size() + 1;
        const auto area_sota   = width_sota * height_sota;
        const auto time_sota   = mockturtle::to_seconds(orthogonal_planar_stats.time_total);

        // calculate bounding box
        const auto bounding_box_proposed = fiction::bounding_box_2d(gate_level_layout_proposed);
        const auto width_proposed  = bounding_box_proposed.get_x_size() + 1;
        const auto height_proposed = bounding_box_proposed.get_y_size() + 1;
        const auto area_proposed   = width_proposed * height_proposed;
        const auto time_proposed   = mockturtle::to_seconds(orthogonal_planar_stats.time_total);

        const auto area_diff = (static_cast<double>(area_sota - area_proposed) / area_sota) * 100.0;
        const auto time_diff = (static_cast<double>(time_sota - time_proposed) / time_sota) * 100.0;

        orthogonal_planar_exp(benchmark, planarized_b.num_pis(), planarized_b.num_pos(),
                              planarized_b.num_gates(), width_sota, height_sota, area_sota, time_sota,
                              width_proposed, height_proposed, area_proposed, time_proposed,
                              area_diff,
                              time_diff, planar_layout,
                              cec_m.value(), eq);

        /* orthogonal_planar_exp(
             benchmark, planarized_b.num_pis(), planarized_b.num_pos(), planarized_b.num_gates(), gate_level_layout.x(),
             gate_level_layout.y(), 0, 0, 0, 0, mockturtle::to_seconds(orthogonal_planar_stats.time_total),
             mockturtle::to_seconds(post_layout_optimization_stats.time_total), 0, planar_layout, cec_m.value(), eq,
           0);*/

        orthogonal_planar_exp.save();
        orthogonal_planar_exp.table();
    }

    return EXIT_SUCCESS;
}