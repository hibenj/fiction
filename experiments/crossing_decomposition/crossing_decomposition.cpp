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
 * nodes where that lowers the crossing count; every result is checked by SAT.
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
#include <fiction/verification/virtual_miter.hpp>

#include <fmt/format.h>
#include <mockturtle/algorithms/cleanup.hpp>
#include <mockturtle/algorithms/equivalence_checking.hpp>
#include <mockturtle/algorithms/lut_mapper.hpp>
#include <mockturtle/networks/aig.hpp>
#include <mockturtle/networks/klut.hpp>
#include <mockturtle/utils/stopwatch.hpp>

#include <cstdint>
#include <exception>
#include <sstream>
#include <stdexcept>
#include <string>
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
 * Checks equivalence of the decomposed network against the balanced one by SAT on the `virtual_miter`.
 */
template <typename Spec, typename Impl>
bool is_equivalent(const Spec& spec, const Impl& impl)
{
    const auto miter = virtual_miter<technology_network>(spec, impl);

    if (!miter.has_value())
    {
        return false;
    }

    mockturtle::equivalence_checking_stats st{};
    const auto                             cec = mockturtle::equivalence_checking(*miter, {}, &st);

    return cec.has_value() && *cec;
}

}  // namespace

int main(const int argc, const char** argv)  // NOLINT
{
    experiments::experiment<std::string, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint64_t, uint64_t,
                            uint32_t, uint32_t, uint64_t, double, bool>
        decomposition_exp{"crossing_decomposition",
                          "benchmark",
                          "inputs",
                          "outputs",
                          "LUTs",
                          "LUTs after",
                          "depth",
                          "depth after",
                          "crossings",
                          "crossings after",
                          "shared nodes",
                          "levels inserted",
                          "swaps",
                          "runtime [s]",
                          "equivalent"};

    // benchmark names on the command line restrict the run, e.g., `ISCAS85/c17`
    std::vector<std::string> benchmarks{};
    if (argc > 1)
    {
        for (int i = 1; i < argc; ++i)
        {
            benchmarks.emplace_back(argv[i]);
        }
    }
    else
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
            const auto klut                    = mockturtle::lut_map(aig, map_ps);

            const auto mapped   = convert_network<technology_network>(klut);
            const auto balanced = network_balancing<technology_network>(mapped);

            fmt::print("[i]   decomposing {} LUTs in {} balanced nodes\n", num_luts(balanced), balanced.num_gates());

            crossing_decomposition_params ps{};
            crossing_decomposition_stats  st{};
            const auto                    result = crossing_decomposition(balanced, ps, &st);

            fmt::print("[i]   checking equivalence\n");
            const auto equivalent = is_equivalent(balanced, result);

            if (!equivalent)
            {
                ++failures;
                fmt::print("[e] {}: not equivalent\n", benchmark);
            }

            decomposition_exp(benchmark, balanced.num_pis(), balanced.num_pos(), num_luts(balanced), num_luts(result),
                              mutable_rank_view{balanced}.depth(), result.depth(), st.crossings_before,
                              st.crossings_after, st.decompositions, st.levels_inserted, st.swaps,
                              mockturtle::to_seconds(st.time_total), equivalent);

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
