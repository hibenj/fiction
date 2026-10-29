/*
 * Copyright (c) 2018 - 2023 Marcel Walter
 * Copyright (c) 2023 - present Chair for Design Automation, Technical University of Munich
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

/**
 * @file
 * @brief Compares the gap-propagation placement with PLANE over benchmark networks and verifies every layout.
 * @author Benjamin Hien (hibenj)
 */

#include "fiction_experiments.hpp"

#include <fiction/layouts/gate_layout.hpp>
#include <fiction/networks/extract_layout_network.hpp>
#include <fiction/networks/io/network_reader.hpp>
#include <fiction/networks/technology_network.hpp>
#include <fiction/networks/views/mutable_rank_view.hpp>
#include <fiction/physical_design/orthogonal_planar.hpp>
#include <fiction/physical_design/plane.hpp>
#include <fiction/synthesis/network_balancing.hpp>
#include <fiction/synthesis/planarization.hpp>
#include <fiction/types.hpp>
#include <fiction/verification/equivalence_checking.hpp>

#include <fmt/format.h>
#include <kitty/partial_truth_table.hpp>
#include <mockturtle/algorithms/cleanup.hpp>
#include <mockturtle/algorithms/simulation.hpp>
#include <mockturtle/utils/stopwatch.hpp>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

using namespace fiction;
using namespace fiction::layouts;
using namespace fiction::networks;
using namespace fiction::networks::io;
using namespace fiction::networks::views;
using namespace fiction::synthesis;
using namespace fiction::physical_design;
using namespace fiction::verification;

namespace
{

/**
 * Balanced networks above this size are skipped.
 */
constexpr uint32_t MAX_BALANCED_NODES = 8000;
/**
 * Layouts up to this many nodes are checked with SAT, larger ones by simulation.
 */
constexpr uint32_t MAX_NODES_FOR_SAT = 200000;
/**
 * Layouts above this many gates are not checked for equivalence: the simulation of such a layout needs several
 * gigabytes. They are still checked for crossings, and the table reports them as unchecked.
 */
constexpr uint32_t MAX_NODES_FOR_EQUIVALENCE = 1000000;
/**
 * Random patterns of the simulation fallback.
 */
constexpr uint32_t SIMULATION_PATTERNS = 4096;
/**
 * Duplication cap of the pipeline.
 */
constexpr uint64_t MAX_DUPLICATIONS = 2000000;

tec_nt read_network(const std::string& name)
{
    std::ostringstream      os{};
    network_reader<tec_ptr> reader{fiction_experiments::benchmark_path(name), os};

    return *reader.get_networks().front();
}

/**
 * Compares the layout with the planar network it was built from. Both have the same inputs in the same order, so a
 * plain miter suffices. Returns `std::nullopt` for a layout above `MAX_NODES_FOR_EQUIVALENCE` gates.
 */
template <typename Ntk, typename Lyt>
std::optional<bool> layout_equivalent(const Ntk& ntk, const Lyt& lyt)
{
    if (lyt.num_gates() > MAX_NODES_FOR_EQUIVALENCE)
    {
        return std::nullopt;
    }

    const auto extracted = extract_layout_network(lyt);

    if (extracted.size() > MAX_NODES_FOR_SAT)
    {
        using tt_t = kitty::partial_truth_table;
        const mockturtle::partial_simulator sim{ntk.num_pis(), SIMULATION_PATTERNS};

        return mockturtle::simulate<tt_t>(ntk, sim) == mockturtle::simulate<tt_t>(extracted, sim);
    }

    return equivalence_checking(ntk, extracted) != eq_type::NO;
}

/**
 * Outcome of one placement algorithm on one benchmark.
 */
struct placement_result
{
    uint64_t            x{0};
    uint64_t            y{0};
    uint64_t            crossings{0};
    double              seconds{0.0};
    std::optional<bool> equivalent{false};
    std::string         error{};

    [[nodiscard]] uint64_t area() const noexcept
    {
        return x * y;
    }

    [[nodiscard]] std::string equivalence_cell() const
    {
        if (!error.empty())
        {
            return "failed";
        }
        if (!equivalent.has_value())
        {
            return "unchecked";
        }

        return *equivalent ? "yes" : "no";
    }

    [[nodiscard]] bool failed() const noexcept
    {
        return !error.empty() || crossings != 0 || !equivalent.value_or(true);
    }
};

/**
 * Runs one placement algorithm and verifies the layout.
 */
template <typename Ntk, typename Place>
placement_result run_placement(const Ntk& planar, const Place& place)
{
    placement_result result{};

    try
    {
        const auto [layout, seconds] = place();

        result.x         = layout.width();
        result.y         = layout.height();
        result.crossings = layout.num_crossings();
        result.seconds   = seconds;

        result.equivalent = layout_equivalent(planar, layout);
    }
    catch (const std::exception& e)
    {
        result.error = e.what();
    }

    return result;
}

}  // namespace

/**
 * @brief Planarizes benchmark networks, places and routes them with the gap-propagation placement and with PLANE, and
 * verifies every layout.
 *
 * This program balances the benchmark networks of the trindade16, fontes18, ISCAS85, EPFL, and IWLS93 suites whose
 * balanced network has at most 8000 nodes, planarizes them, places the planar network with both algorithms, and checks
 * both layouts for crossings and for equivalence with the planar network. The table reports both layout sizes, the
 * area ratio, and the rounds the gap propagation needed; it is saved as `orthogonal_planar.json`.
 *
 * @param argc The number of command-line arguments.
 * @param argv An array of C-style strings containing the command-line arguments.
 * @return `EXIT_SUCCESS` if every benchmark was placed by both algorithms without crossings and passed every
 * equivalence check that ran, `EXIT_FAILURE` otherwise. A gap propagation that exhausts its round budget counts as a
 * failure and is reported in the error column.
 *
 * Command-line Options:
 *   <filter>      Runs only the benchmarks whose name contains the given text; "" runs all of them. The word
 *                 `paper` runs the 33 benchmarks of the IEEE NANO 2025 paper (trindade16, fontes18, and ten IWLS93
 *                 networks) without the balanced-node cap.
 *   xor           Builds the crossing gates from XOR gates instead of AND, OR, and NOT gates.
 *   --help, -h    Prints the usage and exits.
 *
 * Example Usage:
 *   To run the trindade16 benchmarks:
 *   ./orthogonal_planar trindade16
 */
int main(const int argc, const char** argv)
try
{
    const std::span<const char*> args{argv, static_cast<std::size_t>(argc)};

    if (args.size() > 3 || (args.size() == 3 && std::string{args[2]} != "xor") ||
        (args.size() == 2 && (std::string{args[1]} == "--help" || std::string{args[1]} == "-h")))
    {
        fmt::print("Usage: orthogonal_planar [<filter> [xor]]\n"
                   "  <filter>      Runs only the benchmarks whose name contains the given text; \"\" runs all.\n"
                   "                `paper` runs the 33 benchmarks of the IEEE NANO 2025 paper without the node cap.\n"
                   "  xor           Builds the crossing gates from XOR gates instead of AND, OR, and NOT gates.\n"
                   "  --help, -h    Prints this usage and exits.\n");

        return args.size() == 2 ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    const std::string filter    = args.size() > 1 ? args[1] : "";
    const bool        xor_gates = args.size() > 2;

    if (filter.empty())
    {
        fmt::print("[i] running all benchmarks; pass a name filter to restrict the run, or --help for the usage\n");
    }

    experiments::experiment<std::string, uint32_t, uint32_t, uint64_t, uint64_t, uint64_t, double, std::string,
                            uint64_t, uint64_t, uint64_t, uint32_t, double, std::string, double, std::string>
        exp{"orthogonal_planar", "benchmark", "balanced nodes",   "planar nodes",  "plane x", "plane y",
            "plane area",        "plane (s)", "plane equivalent", "op x",          "op y",    "op area",
            "op rounds",         "op (s)",    "op equivalent",    "area op/plane", "error"};

    // the benchmark set of the IEEE NANO 2025 paper: all of trindade16 and fontes18 plus ten IWLS93 networks; the
    // paper's b1_r1 ships as b1_r2 and its c880 as C880
    const bool paper_set = filter == "paper";

    auto benchmarks =
        fiction_experiments::all_benchmarks(fiction_experiments::trindade16 | fiction_experiments::fontes18);
    if (paper_set)
    {
        for (const auto* name : {"x4", "duke2", "rd84", "t481", "C880", "vda", "table5", "table3", "apex3", "cordic"})
        {
            benchmarks.emplace_back(fmt::format("IWLS93/{}", name));
        }
    }
    else
    {
        benchmarks        = fiction_experiments::all_benchmarks();
        const auto iwls93 = fiction_experiments::iwls93_benchmarks();
        benchmarks.insert(benchmarks.end(), iwls93.cbegin(), iwls93.cend());
    }

    uint32_t failures = 0;

    for (const auto& benchmark : benchmarks)
    {
        if (!paper_set && !filter.empty() && benchmark.find(filter) == std::string::npos)
        {
            continue;
        }

        fmt::print("[i] processing {}\n", benchmark);

        try
        {
            const auto ntk = mockturtle::cleanup_dangling(read_network(benchmark));

            network_balancing_params b_ps{};
            b_ps.unify_outputs           = true;
            b_ps.buffer_constant_outputs = false;
            const auto balanced          = network_balancing<technology_network>(ntk, b_ps);

            if (!paper_set && balanced.size() > MAX_BALANCED_NODES)
            {
                fmt::print("[w] skipping {} with {} balanced nodes\n", benchmark, balanced.size());
                continue;
            }

            const mutable_rank_view ranked{balanced};

            fmt::print("[i]   planarizing {} nodes\n", balanced.size());
            planarization_params p_ps{};
            p_ps.duplication.max_duplications         = MAX_DUPLICATIONS;
            p_ps.duplication.crossing_gates.xor_gates = xor_gates;
            const auto planar                         = planarization(ranked, p_ps);

            fmt::print("[i]   PLANE on {} planar nodes\n", planar.size());
            const auto plane_result =
                run_placement(planar,
                              [&planar]()
                              {
                                  plane_stats st{};
                                  auto        layout = plane<cartesian_gate_layout>(planar, {}, &st);
                                  return std::pair{std::move(layout), mockturtle::to_seconds(st.time_total)};
                              });

            fmt::print("[i]   gap propagation on {} planar nodes\n", planar.size());
            orthogonal_planar_stats op_st{};
            const auto              op_result =
                run_placement(planar,
                              [&planar, &op_st]()
                              {
                                  auto layout = orthogonal_planar<cartesian_gate_layout>(planar, {}, &op_st);
                                  return std::pair{std::move(layout), mockturtle::to_seconds(op_st.time_total)};
                              });

            if (plane_result.failed() || op_result.failed())
            {
                ++failures;
            }

            std::string error{};
            if (!plane_result.error.empty())
            {
                error += "plane: " + plane_result.error + " ";
            }
            if (!op_result.error.empty())
            {
                error += "op: " + op_result.error + " ";
            }
            if (plane_result.crossings != 0 || op_result.crossings != 0)
            {
                error += fmt::format("crossings plane {} op {}", plane_result.crossings, op_result.crossings);
            }

            const double ratio = plane_result.area() != 0 && op_result.error.empty() ?
                                     static_cast<double>(op_result.area()) / static_cast<double>(plane_result.area()) :
                                     0.0;

            exp(benchmark, balanced.size(), planar.size(), plane_result.x, plane_result.y, plane_result.area(),
                plane_result.seconds, plane_result.equivalence_cell(), op_result.x, op_result.y, op_result.area(),
                op_st.rounds, op_result.seconds, op_result.equivalence_cell(), ratio, error);
            exp.save();
            exp.table();
        }
        catch (const std::exception& e)
        {
            fmt::print("[e] {} failed: {}\n", benchmark, e.what());
            ++failures;
        }
    }

    fmt::print("[i] {} benchmark(s) with failures\n", failures);

    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
catch (const std::exception& exception)
{
    static_cast<void>(std::fputs("[e] Experiment failed: ", stderr));
    static_cast<void>(std::fputs(exception.what(), stderr));
    static_cast<void>(std::fputc('\n', stderr));
    return EXIT_FAILURE;
}
