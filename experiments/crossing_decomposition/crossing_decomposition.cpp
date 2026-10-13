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
 * @brief Maps benchmark networks to 4-LUTs, balances them, and decomposes LUT pairs with shared fanins into shared
 * nodes where that lowers the crossing count; every result is checked by SAT or, where SAT gives up, by random
 * simulation. `--accept-all` decomposes every pair with a shared function, as in the original work.
 * @author Benjamin Hien (hibenj)
 */

#include "fiction_experiments.hpp"

#include <fiction/networks/io/network_reader.hpp>
#include <fiction/networks/technology_network.hpp>
#include <fiction/networks/views/mutable_rank_view.hpp>
#include <fiction/synthesis/crossing_decomposition.hpp>
#include <fiction/synthesis/network_balancing.hpp>
#include <fiction/synthesis/network_conversion.hpp>
#include <fiction/types.hpp>
#include <fiction/utils/graph/mincross.hpp>
#include <fiction/verification/virtual_miter.hpp>

#include <fmt/format.h>
#include <kitty/operations.hpp>
#include <kitty/partial_truth_table.hpp>
#include <mockturtle/algorithms/cleanup.hpp>
#include <mockturtle/algorithms/equivalence_checking.hpp>
#include <mockturtle/algorithms/lut_mapper.hpp>
#include <mockturtle/algorithms/simulation.hpp>
#include <mockturtle/utils/node_map.hpp>
#include <mockturtle/utils/stopwatch.hpp>
#include <mockturtle/views/topo_view.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iterator>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

using namespace fiction;
using namespace fiction::networks;
using namespace fiction::networks::io;
using namespace fiction::networks::views;
using namespace fiction::synthesis;
using namespace fiction::verification;

namespace
{

/**
 * The benchmarks of the paper: ISCAS85 and the small EPFL circuits.
 */
constexpr uint64_t PAPER_BENCHMARKS =
    fiction_experiments::iscas85 | fiction_experiments::adder | fiction_experiments::cavlc | fiction_experiments::ctrl |
    fiction_experiments::dec | fiction_experiments::i2c | fiction_experiments::int2float |
    fiction_experiments::priority | fiction_experiments::router;
/**
 * LUT size of the mapping.
 */
constexpr uint32_t LUT_SIZE = 4;
/**
 * Command-line option that switches the improvement requirement off.
 */
constexpr std::string_view ACCEPT_ALL_OPTION = "--accept-all";
/**
 * Conflicts after which the SAT-based equivalence check gives way to random simulation.
 */
constexpr uint32_t SAT_CONFLICT_LIMIT = 1000000;
/**
 * Number of random input patterns of the simulation-based check.
 */
constexpr uint32_t SIMULATION_PATTERNS = 4096;

/**
 * Reads a benchmark as an AIG.
 *
 * @param name Benchmark name relative to `benchmarks/`, without extension.
 * @return The network.
 */
aig_nt read_aig(const std::string& name)
{
    std::ostringstream      os{};
    network_reader<aig_ptr> reader{fiction_experiments::benchmark_path(name), os};

    if (reader.get_networks().empty())
    {
        throw std::runtime_error{
            fmt::format("could not read {}: {}", fiction_experiments::benchmark_path(name), os.str())};
    }

    return *reader.get_networks().front();
}

/**
 * Rebuilds a LUT network so that every node's function has exactly one variable per fanin. `lut_map` can leave a
 * function with more variables than the node has fanins; mockturtle evaluates such a node with the extra variables
 * at zero in simulation but not in its CNF encoding, so the mapped network is not even equivalent to its own AIG
 * under SAT. Shrinking the function to the fanins restores the function the mapper meant.
 */
mockturtle::klut_network normalize(const mockturtle::klut_network& klut)
{
    mockturtle::klut_network                                                         dest{};
    mockturtle::node_map<mockturtle::klut_network::signal, mockturtle::klut_network> old2new{klut};

    old2new[klut.get_constant(false)] = dest.get_constant(false);
    old2new[klut.get_constant(true)]  = dest.get_constant(true);
    klut.foreach_pi([&](const auto& n) { old2new[n] = dest.create_pi(); });

    const mockturtle::topo_view topo{klut};
    topo.foreach_gate(
        [&](const auto& n)
        {
            std::vector<mockturtle::klut_network::signal> children{};
            klut.foreach_fanin(n, [&](const auto& f) { children.push_back(old2new[klut.get_node(f)]); });

            auto f = klut.node_function(n);
            if (f.num_vars() > children.size())
            {
                f = kitty::shrink_to(f, static_cast<uint32_t>(children.size()));
            }

            old2new[n] = dest.create_node(children, f);
        });

    klut.foreach_po([&](const auto& po) { dest.create_po(old2new[klut.get_node(po)]); });

    return dest;
}

/**
 * Counts the gates of a network that are not buffers.
 */
template <typename Ntk>
uint32_t num_luts(const Ntk& ntk)
{
    uint32_t count = 0;

    ntk.foreach_gate(
        [&ntk, &count](const auto& n)
        {
            if (!ntk.is_buf(n))
            {
                ++count;
            }
        });

    return count;
}

/**
 * Result of an equivalence check.
 */
struct equivalence_result
{
    /**
     * Whether the networks were found equivalent.
     */
    bool equivalent{false};
    /**
     * `SAT` for a proof, `simulation` for a random-pattern check, `miter` if no miter could be built.
     */
    std::string method{"miter"};
};

/**
 * Simulates `SIMULATION_PATTERNS` random patterns on both networks and compares the outputs. Used where SAT runs
 * into its conflict limit, e.g., on multipliers.
 */
template <typename Spec, typename Impl>
bool simulate_equivalent(const Spec& spec, const Impl& impl)
{
    using tt_t = kitty::partial_truth_table;

    const mockturtle::partial_simulator sim{spec.num_pis(), SIMULATION_PATTERNS};

    // the plain network underneath the rank view: its `foreach_pi` runs in creation order, as the simulator expects
    const auto reference = mockturtle::simulate<tt_t>(spec, sim);
    const auto result    = mockturtle::simulate<tt_t>(static_cast<const technology_network&>(impl), sim);

    return reference == result;
}

/**
 * Counts the crossings of a ranked network in its current order.
 */
template <typename Ntk>
uint64_t count_crossings(const Ntk& ntk)
{
    utils::graph::mincross_params ps{};
    ps.optimize = false;
    utils::graph::mincross_stats st{};

    utils::graph::mincross(ntk, ps, &st);

    return st.num_crossings;
}

/**
 * Checks equivalence of the decomposed network against the balanced one: by SAT on the `virtual_miter` up to
 * `SAT_CONFLICT_LIMIT` conflicts, by random simulation if SAT gives up.
 */
template <typename Spec, typename Impl>
equivalence_result is_equivalent(const Spec& spec, const Impl& impl)
{
    const auto miter = virtual_miter<technology_network>(spec, impl);

    if (!miter.has_value())
    {
        return {};
    }

    mockturtle::equivalence_checking_params ps{};
    ps.conflict_limit = SAT_CONFLICT_LIMIT;

    mockturtle::equivalence_checking_stats st{};
    const auto                             cec = mockturtle::equivalence_checking(*miter, ps, &st);

    if (cec.has_value())
    {
        return {*cec, "SAT"};
    }

    return {simulate_equivalent(spec, impl), "simulation"};
}

}  // namespace

int main(const int argc, const char** argv)  // NOLINT
{
    // `--accept-all` decomposes every pair with a shared function, as in the original work, instead of only the
    // pairs that lower the crossing count
    const std::span<const char*> all_args{argv, static_cast<std::size_t>(argc)};
    const bool                   accept_all =
        std::ranges::any_of(all_args, [](const char* arg) { return std::string_view{arg} == ACCEPT_ALL_OPTION; });

    experiments::experiment<std::string, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint64_t, uint64_t,
                            uint64_t, uint32_t, uint32_t, uint64_t, uint64_t, double, bool, std::string>
        decomposition_exp{accept_all ? "crossing_decomposition_accept_all" : "crossing_decomposition",
                          "benchmark",
                          "inputs",
                          "outputs",
                          "LUTs",
                          "LUTs after",
                          "depth",
                          "depth after",
                          "crossings unordered",
                          "crossings",
                          "crossings after",
                          "shared nodes",
                          "levels inserted",
                          "swaps",
                          "candidates",
                          "runtime [s]",
                          "equivalent",
                          "check"};

    // benchmark names on the command line restrict the run, e.g., `ISCAS85/c17`
    std::vector<std::string> benchmarks{};
    if (argc > 1)
    {
        const std::span<const char*> args{argv, static_cast<std::size_t>(argc)};
        benchmarks.assign(std::next(args.begin()), args.end());
        std::erase(benchmarks, ACCEPT_ALL_OPTION);
    }
    if (benchmarks.empty())
    {
        benchmarks = fiction_experiments::all_benchmarks(PAPER_BENCHMARKS);
    }

    uint32_t failures = 0;

    for (const auto& benchmark : benchmarks)
    {
        fmt::print("[i] processing {}\n", benchmark);

        try
        {
            const auto aig = mockturtle::cleanup_dangling(read_aig(benchmark));

            mockturtle::lut_map_params map_ps{};
            map_ps.cut_enumeration_ps.cut_size = LUT_SIZE;
            const auto klut                    = normalize(mockturtle::lut_map(aig, map_ps));

            const auto mapped   = convert_network<technology_network>(klut);
            const auto balanced = network_balancing<technology_network>(mapped);

            fmt::print("[i]   decomposing {} LUTs in {} balanced nodes\n", num_luts(balanced), balanced.num_gates());

            const auto unordered = count_crossings(mutable_rank_view{balanced});

            crossing_decomposition_params ps{};
            ps.require_improvement = !accept_all;

            crossing_decomposition_stats st{};
            const auto                   result = crossing_decomposition(balanced, ps, &st);

            st.report();

            fmt::print("[i]   checking equivalence\n");
            const auto equivalent = is_equivalent(balanced, result);

            if (!equivalent.equivalent)
            {
                ++failures;
                fmt::print("[e] {}: not equivalent\n", benchmark);
            }

            decomposition_exp(benchmark, balanced.num_pis(), balanced.num_pos(), num_luts(balanced), num_luts(result),
                              mutable_rank_view{balanced}.depth(), result.depth(), unordered, st.crossings_before,
                              st.crossings_after, st.decompositions, st.levels_inserted, st.swaps, st.candidates,
                              mockturtle::to_seconds(st.time_total), equivalent.equivalent, equivalent.method);

            decomposition_exp.save();
            decomposition_exp.table();
        }
        catch (const std::exception& e)
        {
            ++failures;
            fmt::print("[e] {}: {}\n", benchmark, e.what());
        }
    }

    fmt::print("[i] {} of {} benchmarks failed\n", failures, benchmarks.size());

    return failures == 0 ? 0 : 1;
}
