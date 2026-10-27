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
 * @brief Compares two flows that end in a planar, balanced, fanout-substituted logic network, on the same input.
 *
 * The control flow is the existing one: the AIG as a network of gates and inverters, balanced, ordered by
 * `mincross`, planarized by `planarization` (hybrid node duplication, crossing gates, planar fanout substitution,
 * planar rebalancing) and scheduled by `planar_scheduling`. The new flow maps the AIG to 4-LUTs, balances with
 * shared chains, orders by `mincross`, runs the crossing decomposition, replaces every LUT by its planar cell
 * (`planar_cell_substitution`) and then planarizes and schedules the cell network with the same two steps. Every
 * result is checked planar, balanced with unified outputs, fanout-substituted and equivalent to the AIG.
 *
 * Options: `--input-dir <dir>` reads AIGER files `<dir>/<set>/<name>.aig` or `<dir>/<set>__<name>.aig` instead of
 * the shipped optimized AIGs (`benchmarks/optimized`, which replace the Verilog benchmarks where they exist); `--cells
 * <file>` loads a cell database instead of generating the default one ({AND, OR, XOR}, four levels, the 529 remaining
 * classes filled in at five), `--maj` generates the {AND, OR, XOR, MAJ} database instead (four levels, one class at
 * five); `--max-nodes <n>` skips the planarization of networks with more nodes (default 14000);
 * `--private-chains` balances the control with one chain per edge, as the planarization experiment does, instead of
 * shared chains; `--no-schedule` leaves the scheduler out; `--exact-padding` lets the hybrid strategy charge a
 * crossing level by the passes its gadgets need, `--greedy` schedules the gadgets greedily instead of by odd-even
 * transposition rounds, `--best-schedule` takes the fewer passes of both per rank and `--crossing-cost-scale <x>`
 * scales the cost of keeping a level's crossings in the hybrid decision; the inverted edges of the cells are folded
 * into the gates that read them (the Bestagon skeleton library's two-input functions), `--inverter-nodes` keeps them
 * as inverter nodes instead; `--cells-only` skips the control flow (its columns stay zero); benchmark names restrict
 * the run. The defaults are the best variant measured (project 6, M3): the {AND, OR, XOR} database with absorbed
 * inversions.
 */

#include "fiction_experiments.hpp"

#include <fiction/networks/io/network_reader.hpp>
#include <fiction/networks/technology_network.hpp>
#include <fiction/networks/views/mutable_rank_view.hpp>
#include <fiction/synthesis/crossing_decomposition.hpp>
#include <fiction/synthesis/fanout_substitution.hpp>
#include <fiction/synthesis/io/planar_cell_database_json.hpp>
#include <fiction/synthesis/network_balancing.hpp>
#include <fiction/synthesis/network_conversion.hpp>
#include <fiction/synthesis/planar_cell_database.hpp>
#include <fiction/synthesis/planar_cell_substitution.hpp>
#include <fiction/synthesis/planar_scheduling.hpp>
#include <fiction/synthesis/planarization.hpp>
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
#include <mockturtle/networks/klut.hpp>
#include <mockturtle/utils/node_map.hpp>
#include <mockturtle/utils/stopwatch.hpp>
#include <mockturtle/views/fanout_view.hpp>
#include <mockturtle/views/topo_view.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
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
 * The benchmarks of the crossing decomposition experiment: ISCAS85 and the small EPFL circuits.
 */
constexpr uint64_t PAPER_BENCHMARKS =
    fiction_experiments::iscas85 | fiction_experiments::adder | fiction_experiments::cavlc | fiction_experiments::ctrl |
    fiction_experiments::dec | fiction_experiments::i2c | fiction_experiments::int2float |
    fiction_experiments::priority | fiction_experiments::router;

constexpr uint32_t LUT_SIZE = 4;

constexpr std::string_view INPUT_DIR_OPTION      = "--input-dir";
constexpr std::string_view CELLS_OPTION          = "--cells";
constexpr std::string_view MAX_NODES_OPTION      = "--max-nodes";
constexpr std::string_view PRIVATE_CHAINS_OPTION = "--private-chains";
constexpr std::string_view NO_SCHEDULE_OPTION    = "--no-schedule";
constexpr std::string_view EXACT_PADDING_OPTION  = "--exact-padding";
constexpr std::string_view GREEDY_OPTION         = "--greedy";
constexpr std::string_view BEST_OPTION           = "--best-schedule";
constexpr std::string_view COST_SCALE_OPTION     = "--crossing-cost-scale";
constexpr std::string_view INVERTER_NODES_OPTION = "--inverter-nodes";
constexpr std::string_view MAJ_OPTION            = "--maj";
constexpr std::string_view CELLS_ONLY_OPTION     = "--cells-only";
/**
 * Command-line option that names a directory of BLIF files (`<set>__<name>.blif`) already mapped to LUTs of at most
 * LUT_SIZE inputs, e.g., by ABC. The cell flow then starts from that network instead of mapping the AIG itself; the
 * control flow and the equivalence checks still use the benchmark's AIG.
 */
constexpr std::string_view BLIF_DIR_OPTION       = "--blif-dir";

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

aig_nt read_aig_file(const std::string& dir, const std::string& benchmark)
{
    // `<dir>/<set>/<name>.aig` as in `benchmarks/optimized`, or the flat `<dir>/<set>__<name>.aig`
    auto flat = benchmark;
    std::replace(flat.begin(), flat.end(), '/', '_');
    flat.insert(flat.find('_'), "_");

    for (const auto& path : {fmt::format("{}/{}.aig", dir, benchmark), fmt::format("{}/{}.aig", dir, flat)})
    {
        if (!std::filesystem::is_regular_file(path))
        {
            continue;
        }
        std::ostringstream      os{};
        network_reader<aig_ptr> reader{path, os};
        if (reader.get_networks().empty())
        {
            throw std::runtime_error{fmt::format("could not read {}: {}", path, os.str())};
        }
        return *reader.get_networks().front();
    }

    throw std::runtime_error{fmt::format("no AIG of {} in {}", benchmark, dir)};
}

/**
 * The benchmark's AIG: from `dir` if given, else the shipped optimized AIG, else the Verilog benchmark.
 */
aig_nt read_benchmark_aig(const std::string& dir, const std::string& benchmark)
{
    if (!dir.empty())
    {
        return read_aig_file(dir, benchmark);
    }
    if (fiction_experiments::has_optimized_benchmark(benchmark))
    {
        return read_aig_file(fmt::format("{}../benchmarks/optimized", EXPERIMENTS_PATH), benchmark);
    }
    return read_aig(benchmark);
}

/**
 * Reads a BLIF file `<dir>/<set>__<name>.blif` of a benchmark as an already mapped LUT network.
 */
technology_network read_blif_file(const std::string& dir, const std::string& benchmark)
{
    auto file = benchmark;
    std::replace(file.begin(), file.end(), '/', '_');
    file.insert(file.find('_'), "_");

    const auto              path = fmt::format("{}/{}.blif", dir, file);
    std::ostringstream      os{};
    network_reader<tec_ptr> reader{path, os};

    if (reader.get_networks().empty())
    {
        throw std::runtime_error{fmt::format("could not read {}: {}", path, os.str())};
    }

    return *reader.get_networks().front();
}

/**
 * Gives every LUT a truth table of exactly its fanin count.
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
            auto tt = klut.node_function(n);
            if (tt.num_vars() > children.size())
            {
                tt = kitty::shrink_to(tt, static_cast<uint32_t>(children.size()));
            }
            old2new[n] = dest.create_node(children, tt);
        });
    klut.foreach_po([&](const auto& po) { dest.create_po(old2new[klut.get_node(po)]); });

    return dest;
}

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
 * SAT with a conflict limit, random simulation as the fallback.
 */
template <typename Spec, typename Impl>
std::string is_equivalent(const Spec& spec, const Impl& impl)
{
    const auto miter = virtual_miter<technology_network>(spec, impl);
    if (!miter.has_value())
    {
        return "no miter";
    }
    mockturtle::equivalence_checking_params ps{};
    ps.conflict_limit = 1000000;
    mockturtle::equivalence_checking_stats st{};
    const auto                             cec = mockturtle::equivalence_checking(*miter, ps, &st);
    if (cec.has_value())
    {
        return *cec ? "SAT" : "DIFFERENT";
    }
    mockturtle::partial_simulator sim{miter->num_pis(), 10000};
    const auto                    tts = mockturtle::simulate<kitty::partial_truth_table>(*miter, sim);
    for (const auto& tt : tts)
    {
        if (kitty::count_ones(tt) != 0)
        {
            return "DIFFERENT (simulation)";
        }
    }
    return "simulation";
}

/**
 * The nodes of a final network by kind.
 */
struct node_kinds
{
    uint64_t gates{0};
    uint64_t inverters{0};
    uint64_t buffers{0};
    uint64_t fanouts{0};
};

template <typename Ntk>
node_kinds count_kinds(const Ntk& ntk)
{
    const mockturtle::fanout_view fo{ntk};
    node_kinds                    k{};
    ntk.foreach_gate(
        [&ntk, &fo, &k](const auto& n)
        {
            if (ntk.is_buf(n))
            {
                if (fo.fanout_size(n) > 1)
                {
                    ++k.fanouts;
                }
                else
                {
                    ++k.buffers;
                }
            }
            else if (ntk.is_inv(n))
            {
                ++k.inverters;
            }
            else
            {
                ++k.gates;
            }
        });
    return k;
}

/**
 * What one flow produces.
 */
struct flow_result
{
    uint64_t    nodes_before{0};  // the ranked network that enters the planarization
    uint64_t    crossings{0};
    uint32_t    depth_before{0};
    uint64_t    planar_nodes{0};
    uint64_t    duplications{0};
    uint64_t    kept_crossings{0};
    uint64_t    scheduled_nodes{0};
    node_kinds  kinds{};
    uint32_t    depth{0};
    bool        ok{false};
    std::string equivalent{"skipped"};
    double      time{0.0};
};

/**
 * Planarizes and schedules a ranked, balanced network with unified outputs, and checks the result.
 */
/**
 * Options of the planarization: `pad_by_passes` of the hybrid cost model and the schedule of the crossing gates.
 */
struct planarization_options
{
    bool   exact_padding{false};
    bool   greedy{false};
    bool   best{false};
    double crossing_cost_scale{1.0};
};

template <typename Ntk>
flow_result finish(const aig_nt& aig, const Ntk& ranked, const uint64_t max_nodes, const bool schedule,
                   const planarization_options& options)
{
    flow_result r{};
    r.nodes_before = ranked.num_gates();
    r.crossings    = count_crossings(ranked);
    r.depth_before = ranked.depth();
    if (r.nodes_before > max_nodes)
    {
        return r;
    }

    mockturtle::stopwatch<>::duration t{};
    {
        const mockturtle::stopwatch stop{t};

        planarization_params pps{};
        pps.duplication.duplication_cost.pad_by_passes = options.exact_padding;
        pps.duplication.crossing_cost_scale            = options.crossing_cost_scale;
        using pass_schedule                            = crossing_gate_planarization_params::pass_schedule;
        pps.duplication.crossing_schedule              = options.best   ? pass_schedule::BEST :
                                                         options.greedy ? pass_schedule::GREEDY :
                                                                          pass_schedule::ODD_EVEN;
        planarization_stats pst{};
        const auto          planar = planarization(ranked, pps, &pst);
        r.planar_nodes             = planar.num_gates();
        r.duplications             = pst.duplication.num_duplications;
        r.kept_crossings           = pst.duplication.num_crossings;

        const auto check = [&aig, &r](const auto& result)
        {
            r.scheduled_nodes = result.num_gates();
            r.kinds           = count_kinds(result);
            r.depth           = result.depth();
            r.ok              = is_balanced(result, {.unify_outputs = true, .buffer_constant_outputs = false}) &&
                                is_fanout_substituted(result) && count_crossings(result) == 0;
            r.equivalent      = is_equivalent(aig, result);
        };
        if (schedule)
        {
            check(planar_scheduling(planar));
        }
        else
        {
            check(planar);
        }
    }
    r.time = mockturtle::to_seconds(t);

    return r;
}

}  // namespace

int main(const int argc, const char** argv)  // NOLINT
{
    const std::span<const char*> all_args{argv, static_cast<std::size_t>(argc)};

    std::string input_dir{};
    std::string blif_dir{};
    std::string cells{};
    uint64_t    max_nodes = 14000;
    const bool  private_chains =
        std::ranges::any_of(all_args, [](const char* arg) { return std::string_view{arg} == PRIVATE_CHAINS_OPTION; });
    const bool schedule =
        !std::ranges::any_of(all_args, [](const char* arg) { return std::string_view{arg} == NO_SCHEDULE_OPTION; });
    planarization_options options{};
    options.exact_padding =
        std::ranges::any_of(all_args, [](const char* arg) { return std::string_view{arg} == EXACT_PADDING_OPTION; });
    options.greedy =
        std::ranges::any_of(all_args, [](const char* arg) { return std::string_view{arg} == GREEDY_OPTION; });
    options.best = std::ranges::any_of(all_args, [](const char* arg) { return std::string_view{arg} == BEST_OPTION; });
    const bool absorb_inversions =
        !std::ranges::any_of(all_args, [](const char* arg) { return std::string_view{arg} == INVERTER_NODES_OPTION; });
    const bool maj = std::ranges::any_of(all_args, [](const char* arg) { return std::string_view{arg} == MAJ_OPTION; });
    const bool cells_only =
        std::ranges::any_of(all_args, [](const char* arg) { return std::string_view{arg} == CELLS_ONLY_OPTION; });
    for (std::size_t i = 1; i + 1 < all_args.size(); ++i)
    {
        const std::string_view arg{all_args[i]};
        if (arg == BLIF_DIR_OPTION)
        {
            blif_dir = all_args[i + 1];
        }
        else if (arg == INPUT_DIR_OPTION)
        {
            input_dir = all_args[i + 1];
        }
        else if (arg == CELLS_OPTION)
        {
            cells = all_args[i + 1];
        }
        else if (arg == MAX_NODES_OPTION)
        {
            max_nodes = std::stoull(all_args[i + 1]);
        }
        else if (arg == COST_SCALE_OPTION)
        {
            options.crossing_cost_scale = std::stod(all_args[i + 1]);
        }
    }

    // benchmark names on the command line restrict the run
    std::vector<std::string> benchmarks{};
    if (argc > 1)
    {
        benchmarks.assign(std::next(all_args.begin()), all_args.end());
        std::erase(benchmarks, PRIVATE_CHAINS_OPTION);
        std::erase(benchmarks, NO_SCHEDULE_OPTION);
        std::erase(benchmarks, EXACT_PADDING_OPTION);
        std::erase(benchmarks, GREEDY_OPTION);
        std::erase(benchmarks, BEST_OPTION);
        std::erase(benchmarks, INVERTER_NODES_OPTION);
        std::erase(benchmarks, MAJ_OPTION);
        std::erase(benchmarks, CELLS_ONLY_OPTION);
        for (std::size_t i = 1; i + 1 < all_args.size(); ++i)
        {
            const std::string_view arg{all_args[i]};
            if (arg == INPUT_DIR_OPTION || arg == BLIF_DIR_OPTION || arg == CELLS_OPTION || arg == MAX_NODES_OPTION ||
                arg == COST_SCALE_OPTION)
            {
                std::erase(benchmarks, std::string{arg});
                std::erase(benchmarks, std::string{all_args[i + 1]});
            }
        }
    }
    if (benchmarks.empty())
    {
        benchmarks = fiction_experiments::all_benchmarks(PAPER_BENCHMARKS);
    }

    // the cell database
    const auto db = [&cells, maj]()
    {
        if (!cells.empty())
        {
            fmt::print("[i] reading cells from {}\n", cells);
            return fiction::synthesis::io::read_planar_cell_database(cells);
        }
        fmt::print("[i] generating cells: {}, four levels, five for the rest\n",
                   maj ? "{AND, OR, XOR, MAJ}" : "{AND, OR, XOR}");
        planar_cell_database_params ps{};
        ps.num_inputs = LUT_SIZE;
        ps.max_levels = 4;
        if (!maj)
        {
            ps.gate_set = {planar_gate::AND, planar_gate::OR, planar_gate::XOR};
        }
        auto generated = generate_planar_cells(ps);
        ps.max_levels  = 5;
        extend_planar_cells(generated, ps);
        return generated;
    }();

    experiments::experiment<std::string, uint32_t, uint32_t, uint64_t, uint64_t, uint32_t, uint64_t, uint64_t, uint64_t,
                            uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint32_t, double, bool, std::string,
                            uint64_t, uint64_t, uint64_t, uint32_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t,
                            uint64_t, uint64_t, uint64_t, uint64_t, uint32_t, double, bool, std::string>
        exp{fmt::format("planar_cell_flow{}{}{}{}{}{}{}{}", private_chains ? "_private_chains" : "",
                        schedule ? "" : "_no_schedule", options.exact_padding ? "_exact_padding" : "",
                        options.best ? "_best_schedule" : (options.greedy ? "_greedy" : ""),
                        options.crossing_cost_scale == 1.0 ?
                            "" :
                            fmt::format("_crossing_cost_scale_{:g}", options.crossing_cost_scale),
                        absorb_inversions ? "" : "_inverter_nodes", maj ? "_maj" : "", cells_only ? "_cells_only" : ""),
            "benchmark",
            "inputs",
            "outputs",
            "control nodes",
            "control crossings",
            "control depth",
            "control planar nodes",
            "control duplications",
            "control kept crossings",
            "control final nodes",
            "control gates",
            "control inverters",
            "control buffers",
            "control fanouts",
            "control final depth",
            "control time [s]",
            "control ok",
            "control equivalent",
            "LUTs",
            "cell nodes",
            "cell crossings",
            "cell levels",
            "cell inverters",
            "new planar nodes",
            "new duplications",
            "new kept crossings",
            "new final nodes",
            "new gates",
            "new inverters",
            "new buffers",
            "new fanouts",
            "new final depth",
            "new time [s]",
            "new ok",
            "new equivalent"};

    for (const auto& benchmark : benchmarks)
    {
        fmt::print("[i] processing {}\n", benchmark);

        try
        {
            const auto aig = mockturtle::cleanup_dangling(read_benchmark_aig(input_dir, benchmark));

            network_balancing_params bps{};
            bps.unify_outputs           = true;
            bps.buffer_constant_outputs = false;
            bps.share_buffers           = !private_chains;

            // the control flow: gates and inverters, balanced, ordered, planarized
            const auto control = [&aig, &bps, max_nodes, schedule, &options, cells_only]()
            {
                if (cells_only)
                {
                    fmt::print("[i]   control flow skipped\n");
                    return flow_result{};
                }
                fmt::print("[i]   control flow\n");
                const auto        gates = convert_network<technology_network>(aig);
                const auto        cbal  = network_balancing<technology_network>(gates, bps);
                mutable_rank_view cranked{cbal};
                const auto        cordered = utils::graph::mincross(cranked, {});
                return finish(aig, cordered, max_nodes, schedule, options);
            }();

            // the new flow: LUTs, shared chains, ordered, decomposed, cells, planarized
            fmt::print("[i]   cell flow\n");
            mockturtle::lut_map_params map_ps{};
            map_ps.cut_enumeration_ps.cut_size = LUT_SIZE;
            const auto mapped = [&aig, &map_ps, &blif_dir, &benchmark]()
            {
                if (!blif_dir.empty())
                {
                    return read_blif_file(blif_dir, benchmark);
                }
                return convert_network<technology_network>(normalize(mockturtle::lut_map(aig, map_ps)));
            }();
            network_balancing_params lps       = bps;
            lps.share_buffers                  = true;
            const auto                    lbal = network_balancing<technology_network>(mapped, lps);
            crossing_decomposition_params dps{};
            dps.node_cost                              = 1.0;
            const auto                      decomposed = crossing_decomposition(lbal, dps);
            planar_cell_substitution_params sps{};
            sps.absorb_inversions = absorb_inversions;
            planar_cell_substitution_stats sst{};
            const auto                     cell_network = planar_cell_substitution(decomposed, db, sps, &sst);
            fmt::print("[i]   cells: {} LUTs ({} kept), {} gates, {} inverters ({} absorbed, {} of the input), "
                       "levels {} -> {}\n",
                       sst.luts, sst.luts_kept, sst.gates, sst.inverters, sst.inversions_absorbed, sst.input_inverters,
                       sst.levels_before, sst.levels_after);
            const auto cell = finish(aig, cell_network, max_nodes, schedule, options);

            exp(benchmark, aig.num_pis(), aig.num_pos(), control.nodes_before, control.crossings, control.depth_before,
                control.planar_nodes, control.duplications, control.kept_crossings, control.scheduled_nodes,
                control.kinds.gates, control.kinds.inverters, control.kinds.buffers, control.kinds.fanouts,
                control.depth, control.time, control.ok, control.equivalent, sst.luts, cell.nodes_before,
                cell.crossings, cell.depth_before, sst.inverters, cell.planar_nodes, cell.duplications,
                cell.kept_crossings, cell.scheduled_nodes, cell.kinds.gates, cell.kinds.inverters, cell.kinds.buffers,
                cell.kinds.fanouts, cell.depth, cell.time, cell.ok, cell.equivalent);
            exp.save();
            exp.table();
        }
        catch (const std::exception& e)
        {
            fmt::print("[e] {}: {}\n", benchmark, e.what());
        }
    }

    return 0;
}
