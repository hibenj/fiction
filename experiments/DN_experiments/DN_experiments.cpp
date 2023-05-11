//
// Created by benjamin on 11.05.23.
//

#if (FICTION_Z3_SOLVER)

#include "../../test/utils/blueprints/majority_network_blueprints.hpp"
#include "../../test/utils/blueprints/network_blueprints.hpp"
#include "fiction/algorithms/physical_design/orthogonal_majority_network.hpp"
#include "fiction_experiments.hpp"

#include <fiction/algorithms/physical_design/color_routing.hpp>  // routing based on graph coloring
#include <fiction/algorithms/physical_design/exact.hpp>          // SMT-based physical design of FCN layouts
#include <fiction/algorithms/physical_design/ortho_ordering_network.hpp>
#include <fiction/algorithms/physical_design/orthogonal.hpp>     // OGD-based physical design of FCN layouts
#include <fiction/types.hpp>                                     // pre-defined types suitable for the FCN domain
#include <fiction/utils/routing_utils.hpp>                       // routing utility functions

#include <fmt/format.h>                                          // output formatting
#include <lorina/lorina.hpp>                                     // Verilog/BLIF/AIGER/... file parsing
#include <mockturtle/io/verilog_reader.hpp>                      // call-backs to read Verilog files into networks
#include <mockturtle/networks/aig.hpp>                           // AND-inverter graphs

#include <cstdlib>
#include <string>
#include <string_view>

using namespace fiction;

using gate_lyt = fiction::gate_level_layout<
    fiction::clocked_layout<fiction::tile_based_layout<fiction::cartesian_layout<fiction::offset::ucoord_t>>>>;

using color_routing_experiment =
    experiments::experiment<std::string, uint32_t, uint32_t, uint32_t, std::string_view, uint64_t, uint64_t, uint64_t,
                            uint32_t, uint32_t, uint64_t, uint64_t, uint64_t, uint64_t, double, double, double, double,
                            bool>;

// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
static fiction::orthogonal_physical_design_stats ortho_stats{};
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

std::string benchmark_path_blif(const std::string& benchmark_name, const std::string& benchmark_folder = "../benchmarks")
{
    return fmt::format("{}{}/{}.blif", EXPERIMENTS_PATH, benchmark_folder, benchmark_name);
}
std::string benchmark_path_bench(const std::string& benchmark_name, const std::string& benchmark_folder = "../benchmarks")
{
    return fmt::format("{}{}/{}.bench", EXPERIMENTS_PATH, benchmark_folder, benchmark_name);
}

template <typename Ntk>
Ntk read_ntk(const std::string& name)
{
    fmt::print("[i] processing {}\n", name);

    Ntk network{};

    const auto read_verilog_result =
        lorina::read_verilog(fiction_experiments::benchmark_path(name), mockturtle::verilog_reader(network));
    assert(read_verilog_result == lorina::return_code::success);

    return network;
}

void ortho_ordering_mcs()
{
    ortho_stats = {};

    // parameters for SAT-based color routing
    fiction::color_routing_params routing_params{};
    routing_params.conduct_partial_routing = true;
    routing_params.crossings               = true;
    routing_params.path_limit              = 75;
    routing_params.engine                  = fiction::graph_coloring_engine::MCS;

    experiments::experiment<std::string, uint32_t, uint32_t, uint32_t, uint32_t, uint64_t, uint64_t, uint64_t, uint32_t, double>
        ordering_exp{"ordering",
                     "benchmark",
                     "inputs",
                     "outputs",
                     "num_gates",
                     "layout width (in tiles)",
                     "layout height (in tiles)",
                     "layout area (in tiles)",
                     "gates",
                     "wires",
                     "runtime (in sec)"};

    constexpr const uint64_t bench_select = fiction_experiments::trindade16;

    for (const auto& benchmark : fiction_experiments::all_benchmarks(bench_select))
    {
        const auto ntk = read_ntk<fiction::tec_nt>(benchmark);

        // perform layout generation with an OGD-based algorithm
        auto lyt = fiction::orthogonal_ordering_network<gate_lyt>(ntk, {}, &ortho_stats);

        fiction::gate_level_drvs(lyt);

        const auto check_colors = [](const auto& ntk)
        {
            auto container = fiction::detail::conditional_coloring(ntk);
            if (fiction::detail::is_east_south_colored(container.color_ntk))
            {
                std::cout << "SUCCESS" << std::endl;
            }
            else
            {
                std::cout << "FAILURE" << std::endl;
            }
        };

        check_colors(fiction::input_ordering_view{mockturtle::fanout_view{
            fiction::fanout_substitution<fiction::technology_network>(ntk)}});

        // fiction::debug::write_dot_layout(lyt);

        ordering_exp(benchmark, ntk.num_pis(), ntk.num_pos(), ntk.num_gates(), lyt.x() + 1, lyt.y() + 1,
                     (lyt.x() + 1) * (lyt.y() + 1), lyt.num_gates(),
                     lyt.num_wires(), mockturtle::to_seconds(ortho_stats.time_total));

        ordering_exp.save();
        ordering_exp.table();
    }
}

void ortho_majority_mcs()
{
    ortho_stats = {};

    experiments::experiment<std::string, uint32_t, uint32_t, uint32_t, uint32_t, uint64_t, uint64_t, uint64_t, uint32_t, double>
        majority_exp{"ordering",
                     "benchmark",
                     "inputs",
                     "outputs",
                     "num_gates",
                     "layout width (in tiles)",
                     "layout height (in tiles)",
                     "layout area (in tiles)",
                     "gates",
                     "wires",
                     "runtime (in sec)"};

    auto r1 = blueprints::maj_random_1<mockturtle::names_view<fiction::technology_network>>();
    auto r2 = blueprints::maj_random_2<mockturtle::names_view<fiction::technology_network>>();
    auto r3 = blueprints::maj_random_3<mockturtle::names_view<fiction::technology_network>>();
    auto r4 = blueprints::maj_random_4<mockturtle::names_view<fiction::technology_network>>();
    auto r5 = blueprints::maj_random_5<mockturtle::names_view<fiction::technology_network>>();
    auto r6 = blueprints::maj_random_6<mockturtle::names_view<fiction::technology_network>>();
    auto r7 = blueprints::maj_random_7<mockturtle::names_view<fiction::technology_network>>();
    auto r8 = blueprints::maj_random_8<mockturtle::names_view<fiction::technology_network>>();
    auto r9 = blueprints::maj_random_9<mockturtle::names_view<fiction::technology_network>>();
    auto r10 = blueprints::maj_random_10<mockturtle::names_view<fiction::technology_network>>();
    auto r11 = blueprints::maj_random_11<mockturtle::names_view<fiction::technology_network>>();

    std::vector<mockturtle::names_view<fiction::technology_network>> bms_tech = {r1, r2, r3, r4, r5, r6, r7, r8, r9, r10, r11};

    std::vector<std::string> const benchmark_names {"r1", "r2", "r3", "r4", "r5", "r6", "r7", "r8", "r9", "r10", "r11"};

    for (int i = 0; i < bms_tech.size()-8; ++i)
    {
        auto bm_tech = bms_tech[i];

        auto bm_name = benchmark_names[i];

        std::cout<<"Size "<<bms_tech.size()<<std::endl;

        using gate_layout = fiction::gate_level_layout<clocked_layout<tile_based_layout<cartesian_layout<offset::ucoord_t>>>>;

        //const auto layout_one = fiction::orthogonal<gate_layout>(bm_tech, {}, &st_tech_one);

        const auto lyt = fiction::orthogonal_majority_network<gate_layout>(bm_tech, {}, &ortho_stats);

        fiction::gate_level_drvs(lyt);

        majority_exp(bm_name, bm_tech.num_pis(), bm_tech.num_pos(), bm_tech.num_gates(), lyt.x() + 1, lyt.y() + 1,
                     (lyt.x() + 1) * (lyt.y() + 1), lyt.num_gates(),
                     lyt.num_wires(), mockturtle::to_seconds(ortho_stats.time_total));

        majority_exp.save();
        majority_exp.table();
    }
}

int main()  // NOLINT
{
    ortho_majority_mcs();

    return EXIT_SUCCESS;
}

#else  // FICTION_Z3_SOLVER

#include <cstdlib>
#include <iostream>

int main()  // NOLINT
{
    std::cerr << "[e] Z3 solver is not available, please install Z3 and recompile the code" << std::endl;

    return EXIT_FAILURE;
}

#endif  // FICTION_Z3_SOLVER
