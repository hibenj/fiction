//
// Created by benjamin on 11.05.23.
//

#if (FICTION_Z3_SOLVER)

#include "fiction_experiments.hpp"

#include <fiction/algorithms/physical_design/color_routing.hpp>      // routing based on graph coloring
#include <fiction/algorithms/physical_design/exact.hpp>              // SMT-based physical design of FCN layouts
#include <fiction/algorithms/physical_design/ortho_ordering_network.hpp>
#include <fiction/algorithms/physical_design/orthogonal.hpp>         // OGD-based physical design of FCN layouts
#include <fiction/types.hpp>                                         // pre-defined types suitable for the FCN domain
#include <fiction/utils/routing_utils.hpp>                           // routing utility functions

#include <fmt/format.h>                                              // output formatting
#include <lorina/lorina.hpp>                                         // Verilog/BLIF/AIGER/... file parsing
#include <mockturtle/io/verilog_reader.hpp>                          // call-backs to read Verilog files into networks
#include <mockturtle/networks/aig.hpp>                               // AND-inverter graphs

#include <cstdlib>
#include <string>
#include <string_view>

using gate_lyt = fiction::gate_level_layout<
    fiction::clocked_layout<fiction::tile_based_layout<fiction::cartesian_layout<fiction::offset::ucoord_t>>>>;

using color_routing_experiment =
    experiments::experiment<std::string, uint32_t, uint32_t, uint32_t, std::string_view, uint64_t, uint64_t, uint64_t,
                            uint32_t, uint32_t, uint64_t, uint64_t, uint64_t, uint64_t, double, double, double, double,
                            bool>;

// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
static fiction::orthogonal_physical_design_stats ortho_stats{};
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

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

int main()  // NOLINT
{
    ortho_ordering_mcs();

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