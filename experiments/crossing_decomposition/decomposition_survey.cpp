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
 * @brief Maps benchmark networks to K-LUTs, decomposes every LUT with more than four inputs into nodes of at most
 * four inputs with one of several strategies (resynthesis functors, exact synthesis, or Ashenhurst decompositions with
 * a bound-set rule), balances and orders the result with `mincross`, and counts the
 * crossings; every result is checked against the K-LUT network by SAT or, where SAT gives up, by random simulation.
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
#include <kitty/dynamic_truth_table.hpp>
#include <kitty/operations.hpp>
#include <kitty/partial_truth_table.hpp>
#include <mockturtle/algorithms/cleanup.hpp>
#include <mockturtle/algorithms/equivalence_checking.hpp>
#include <mockturtle/algorithms/klut_to_graph.hpp>
#include <mockturtle/algorithms/lut_mapper.hpp>
#include <mockturtle/algorithms/miter.hpp>
#include <mockturtle/algorithms/node_resynthesis/bidecomposition.hpp>
#include <mockturtle/algorithms/node_resynthesis/davio.hpp>
#include <mockturtle/algorithms/node_resynthesis/dsd.hpp>
#include <mockturtle/algorithms/node_resynthesis/exact.hpp>
#include <mockturtle/algorithms/node_resynthesis/shannon.hpp>
#include <mockturtle/algorithms/node_resynthesis/sop_factoring.hpp>
#include <mockturtle/algorithms/simulation.hpp>
#include <mockturtle/networks/aig.hpp>
#include <mockturtle/networks/klut.hpp>
#include <mockturtle/networks/xag.hpp>
#include <mockturtle/utils/node_map.hpp>
#include <mockturtle/utils/stopwatch.hpp>
#include <mockturtle/views/depth_view.hpp>
#include <mockturtle/views/topo_view.hpp>

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <functional>
#include <iterator>
#include <optional>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
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
 * The benchmarks of project 1: ISCAS85 and the small EPFL circuits.
 */
constexpr uint64_t PAPER_BENCHMARKS =
    fiction_experiments::iscas85 | fiction_experiments::adder | fiction_experiments::cavlc | fiction_experiments::ctrl |
    fiction_experiments::dec | fiction_experiments::i2c | fiction_experiments::int2float |
    fiction_experiments::priority | fiction_experiments::router;
/**
 * Largest number of inputs a node of a result may have.
 */
constexpr uint32_t TARGET_LUT_SIZE = 4;
/**
 * Conflicts after which the SAT-based equivalence check gives way to random simulation.
 */
constexpr uint32_t SAT_CONFLICT_LIMIT = 200000;
/**
 * Number of random input patterns of the simulation-based check.
 */
constexpr uint32_t SIMULATION_PATTERNS = 4096;
/**
 * Conflicts after which the exact synthesis of one LUT gives up and the fallback strategy takes over.
 */
constexpr int EXACT_CONFLICT_LIMIT = 100000;

/**
 * Reads a benchmark as an AIG.
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
 * Reads a BLIF file (an ABC result) as a technology network.
 */
technology_network read_blif(const std::string& path)
{
    std::ostringstream      os{};
    network_reader<tec_ptr> reader{path, os};

    if (reader.get_networks().empty())
    {
        throw std::runtime_error{fmt::format("could not read {}: {}", path, os.str())};
    }

    return *reader.get_networks().front();
}

/**
 * Rebuilds a LUT network so that every node's function has exactly one variable per fanin. `lut_map` can leave a
 * function with more variables than the node has fanins; mockturtle evaluates such a node with the extra variables
 * at zero in simulation but not in its CNF encoding, so the mapped network is not even equivalent to its own AIG
 * under SAT. Shrinking the function to the fanins (the cofactor with the extra variables at zero) restores the
 * function the mapper meant, which the check against the AIG confirms.
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
 * Maps an AIG to K-LUTs with one variable per fanin.
 */
template <typename Aig>
mockturtle::klut_network map_to_luts(const Aig& aig, const uint32_t k)
{
    mockturtle::lut_map_params ps{};
    ps.cut_enumeration_ps.cut_size = k;

    return normalize(mockturtle::lut_map(aig, ps));
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
 * Largest number of fanins of any gate.
 */
template <typename Ntk>
uint32_t max_fanin(const Ntk& ntk)
{
    uint32_t m = 0;

    ntk.foreach_gate([&ntk, &m](const auto& n) { m = std::max(m, ntk.fanin_size(n)); });

    return m;
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
 * A strategy turns the function of one LUT into a small network of nodes with at most `TARGET_LUT_SIZE` inputs,
 * whose primary inputs are the LUT's fanins in order and whose only primary output is the LUT's function. It
 * returns `std::nullopt` if it cannot handle the function; the caller then uses the fallback.
 */
using builder = std::function<std::optional<mockturtle::klut_network>(const kitty::dynamic_truth_table&)>;

/**
 * Runs a mockturtle resynthesis functor on a small XAG and maps the result to 4-LUTs.
 */
template <typename Functor>
builder via_xag(Functor functor)
{
    return [functor](const kitty::dynamic_truth_table& f) -> std::optional<mockturtle::klut_network>
    {
        mockturtle::xag_network                      xag{};
        std::vector<mockturtle::xag_network::signal> pis{};
        pis.reserve(f.num_vars());

        for (uint32_t i = 0; i < f.num_vars(); ++i)
        {
            pis.push_back(xag.create_pi());
        }

        functor(xag, f, pis.cbegin(), pis.cend(), [&xag](const auto& s) { xag.create_po(s); });

        if (xag.num_pos() == 0)
        {
            return std::nullopt;
        }

        mockturtle::lut_map_params ps{};
        ps.cut_enumeration_ps.cut_size = TARGET_LUT_SIZE;

        return mockturtle::lut_map(xag, ps);
    };
}

/**
 * Exact synthesis of 4-input nodes by SAT, with a conflict limit.
 */
builder exact_4lut()
{
    return [](const kitty::dynamic_truth_table& f) -> std::optional<mockturtle::klut_network>
    {
        mockturtle::exact_resynthesis_params ps{};
        ps.conflict_limit = EXACT_CONFLICT_LIMIT;
        const mockturtle::exact_resynthesis<mockturtle::klut_network> exact{TARGET_LUT_SIZE, ps};

        mockturtle::klut_network                      klut{};
        std::vector<mockturtle::klut_network::signal> pis{};
        pis.reserve(f.num_vars());

        for (uint32_t i = 0; i < f.num_vars(); ++i)
        {
            pis.push_back(klut.create_pi());
        }

        exact(klut, f, pis.cbegin(), pis.cend(), [&klut](const auto& s) { klut.create_po(s); });

        if (klut.num_pos() == 0)
        {
            return std::nullopt;
        }

        return klut;
    };
}

/**
 * The strategies by name.
 */
std::vector<std::pair<std::string, builder>> strategies()
{
    using xag = mockturtle::xag_network;

    static mockturtle::shannon_resynthesis<xag> shannon_fallback{};

    return {{"klut",
             [](const kitty::dynamic_truth_table& f) -> std::optional<mockturtle::klut_network>
             {
                 // the K-LUT itself: the crossings of the K-LUT network, nodes of more than four inputs
                 mockturtle::klut_network                      klut{};
                 std::vector<mockturtle::klut_network::signal> pis{};
                 for (uint32_t i = 0; i < f.num_vars(); ++i)
                 {
                     pis.push_back(klut.create_pi());
                 }
                 klut.create_po(klut.create_node(pis, f));
                 return klut;
             }},
            {"shannon", via_xag(mockturtle::shannon_resynthesis<xag>{})},
            {"davio_pos", via_xag(mockturtle::positive_davio_resynthesis<xag>{})},
            {"davio_neg", via_xag(mockturtle::negative_davio_resynthesis<xag>{})},
            {"dsd_shannon", via_xag(mockturtle::dsd_resynthesis<xag, decltype(shannon_fallback)>{shannon_fallback})},
            {"bidec", via_xag(mockturtle::bidecomposition_resynthesis<xag>{})},
            {"sop", via_xag(mockturtle::sop_factoring<xag>{})},
            {"exact4", exact_4lut()}};
}

/**
 * Result of a decomposition of a whole network.
 */
struct decomposed
{
    technology_network network{};
    /**
     * LUTs the strategy decomposed.
     */
    uint32_t decomposed_luts{0};
    /**
     * LUTs the strategy could not handle, decomposed by the fallback.
     */
    uint32_t fallbacks{0};
};

/**
 * Rebuilds a K-LUT network as a technology network in which every LUT with more than `TARGET_LUT_SIZE` inputs is
 * replaced by the small network the strategy builds for its function (the fallback where the strategy gives up).
 */
decomposed decompose(const mockturtle::klut_network& klut, const builder& strategy, const builder& fallback)
{
    decomposed result{};
    auto&      dest = result.network;

    mockturtle::node_map<technology_network::signal, mockturtle::klut_network> old2new{klut};

    old2new[klut.get_constant(false)] = dest.get_constant(false);
    if (klut.get_node(klut.get_constant(true)) != klut.get_node(klut.get_constant(false)))
    {
        old2new[klut.get_constant(true)] = dest.get_constant(true);
    }

    klut.foreach_pi([&](const auto& n) { old2new[n] = dest.create_pi(); });

    const mockturtle::topo_view topo{klut};

    topo.foreach_gate(
        [&](const auto& n)
        {
            std::vector<technology_network::signal> children{};
            klut.foreach_fanin(n, [&](const auto& f) { children.push_back(old2new[klut.get_node(f)]); });

            auto f = klut.node_function(n);

            // a mapper may leave unused top variables in the function; the node is evaluated with them at zero
            if (f.num_vars() > children.size())
            {
                f = kitty::shrink_to(f, static_cast<uint32_t>(children.size()));
            }

            if (children.size() <= TARGET_LUT_SIZE)
            {
                old2new[n] = dest.create_node(children, f);
                return;
            }

            auto small = strategy(f);

            if (!small.has_value())
            {
                ++result.fallbacks;
                small = fallback(f);
            }
            else
            {
                ++result.decomposed_luts;
            }

            if (!small.has_value())
            {
                throw std::runtime_error{"the fallback strategy failed on a LUT"};
            }

            // clone the small network into the destination; its primary inputs are the LUT's fanins in order
            mockturtle::node_map<technology_network::signal, mockturtle::klut_network> local{*small};

            local[small->get_constant(false)] = dest.get_constant(false);
            if (small->get_node(small->get_constant(true)) != small->get_node(small->get_constant(false)))
            {
                local[small->get_constant(true)] = dest.get_constant(true);
            }

            small->foreach_pi([&](const auto& pi, const auto i) { local[pi] = children.at(i); });

            const mockturtle::topo_view small_topo{*small};
            small_topo.foreach_gate(
                [&](const auto& m)
                {
                    std::vector<technology_network::signal> cs{};
                    small->foreach_fanin(m, [&](const auto& g) { cs.push_back(local[small->get_node(g)]); });

                    auto tt = small->node_function(m);
                    if (tt.num_vars() > cs.size())
                    {
                        tt = kitty::shrink_to(tt, static_cast<uint32_t>(cs.size()));
                    }

                    local[m] = dest.create_node(cs, tt);
                });

            small->foreach_po([&](const auto& po) { old2new[n] = local[small->get_node(po)]; });
        });

    klut.foreach_po([&](const auto& po) { dest.create_po(old2new[klut.get_node(po)]); });

    return result;
}

/**
 * Rule for choosing the bound set of an Ashenhurst decomposition.
 */
enum class bound_set_rule : uint8_t
{
    /**
     * The decomposable subset whose members are shared with the most other LUTs of the same level; larger subsets
     * and lower indices break ties.
     */
    SHARED,
    /**
     * The decomposable subset whose members are shared with the fewest other LUTs of the same level (control).
     */
    UNSHARED,
    /**
     * The first decomposable subset, largest size first, then index order (what a structural decomposer does
     * without any criterion).
     */
    FIRST
};

/**
 * Rebuilds a K-LUT network with every LUT of more than `TARGET_LUT_SIZE` inputs split by disjoint Ashenhurst
 * decompositions f(z, y) = g(z, h(y)), the bound set y chosen by `rule`, repeated on g until every node has at most
 * `TARGET_LUT_SIZE` inputs; a function without any decomposition goes to the fallback strategy. "Shared" counts, for
 * every fanin of the LUT, the other gates of the same level (by depth) that have the same fanin.
 */
decomposed decompose_ashenhurst(const mockturtle::klut_network& klut, const bound_set_rule rule,
                                const builder& fallback)
{
    decomposed result{};
    auto&      dest = result.network;

    const mockturtle::depth_view depth{klut};

    // fanin -> number of gates per level that use it
    std::unordered_map<uint64_t, uint32_t> users{};
    const auto                             key = [](const uint32_t level, const auto& n)
    { return (static_cast<uint64_t>(level) << 32u) | static_cast<uint64_t>(n); };

    klut.foreach_gate(
        [&](const auto& n)
        { klut.foreach_fanin(n, [&](const auto& f) { ++users[key(depth.level(n), klut.get_node(f))]; }); });

    mockturtle::node_map<technology_network::signal, mockturtle::klut_network> old2new{klut};

    old2new[klut.get_constant(false)] = dest.get_constant(false);
    if (klut.get_node(klut.get_constant(true)) != klut.get_node(klut.get_constant(false)))
    {
        old2new[klut.get_constant(true)] = dest.get_constant(true);
    }

    klut.foreach_pi([&](const auto& n) { old2new[n] = dest.create_pi(); });

    // splits f over `children` (with share counts `shares`) recursively; returns the output signal
    std::function<technology_network::signal(kitty::dynamic_truth_table, std::vector<technology_network::signal>,
                                             std::vector<uint32_t>, bool&)>
        split = [&](kitty::dynamic_truth_table f, std::vector<technology_network::signal> children,
                    std::vector<uint32_t> shares, bool& used_fallback) -> technology_network::signal
    {
        const auto k = static_cast<uint32_t>(children.size());

        if (k <= TARGET_LUT_SIZE)
        {
            return dest.create_node(children, f);
        }

        // candidate bound sets: subsets of 2 to min(4, k - 1) inputs
        const auto largest = std::min(TARGET_LUT_SIZE, k - 1u);

        std::optional<std::vector<uint32_t>>      best_ys{};
        std::optional<kitty::dynamic_truth_table> best_h{};
        int64_t                                   best_score = 0;

        for (auto size = largest; size >= 2u; --size)
        {
            for (uint64_t mask = 0; mask < (uint64_t{1} << k); ++mask)
            {
                if (static_cast<uint32_t>(std::popcount(mask)) != size)
                {
                    continue;
                }

                std::vector<uint32_t> ys{};
                int64_t               share = 0;
                for (uint32_t i = 0; i < k; ++i)
                {
                    if ((mask >> i) & 1u)
                    {
                        ys.push_back(i);
                        share += shares[i];
                    }
                }

                const auto h = fiction::synthesis::detail::shared_function(f, ys, f, ys);
                if (!h.has_value())
                {
                    continue;
                }

                if (rule == bound_set_rule::FIRST)
                {
                    best_ys = ys;
                    best_h  = h;
                    break;
                }

                // larger subsets first within equal share, lower index order by enumeration
                const auto score = (rule == bound_set_rule::SHARED ? share : -share) * 16 + static_cast<int64_t>(size);
                if (!best_ys.has_value() || score > best_score)
                {
                    best_ys    = ys;
                    best_h     = h;
                    best_score = score;
                }
            }

            if (rule == bound_set_rule::FIRST && best_ys.has_value())
            {
                break;
            }
        }

        if (!best_ys.has_value())
        {
            used_fallback = true;

            const auto small = fallback(f);
            if (!small.has_value())
            {
                throw std::runtime_error{"the fallback strategy failed on a LUT"};
            }

            mockturtle::node_map<technology_network::signal, mockturtle::klut_network> local{*small};
            local[small->get_constant(false)] = dest.get_constant(false);
            if (small->get_node(small->get_constant(true)) != small->get_node(small->get_constant(false)))
            {
                local[small->get_constant(true)] = dest.get_constant(true);
            }
            small->foreach_pi([&](const auto& pi, const auto i) { local[pi] = children.at(i); });

            const mockturtle::topo_view small_topo{*small};
            small_topo.foreach_gate(
                [&](const auto& m)
                {
                    std::vector<technology_network::signal> cs{};
                    small->foreach_fanin(m, [&](const auto& g) { cs.push_back(local[small->get_node(g)]); });
                    auto tt = small->node_function(m);
                    if (tt.num_vars() > cs.size())
                    {
                        tt = kitty::shrink_to(tt, static_cast<uint32_t>(cs.size()));
                    }
                    local[m] = dest.create_node(cs, tt);
                });

            technology_network::signal out{};
            small->foreach_po([&](const auto& po) { out = local[small->get_node(po)]; });
            return out;
        }

        const auto g = fiction::synthesis::detail::outer_function(f, *best_ys, *best_h);

        std::vector<technology_network::signal> h_children{};
        for (const auto y : *best_ys)
        {
            h_children.push_back(children[y]);
        }
        const auto h_signal = dest.create_node(h_children, *best_h);

        std::vector<technology_network::signal> g_children{};
        std::vector<uint32_t>                   g_shares{};
        for (uint32_t i = 0; i < k; ++i)
        {
            if (std::find(best_ys->cbegin(), best_ys->cend(), i) == best_ys->cend())
            {
                g_children.push_back(children[i]);
                g_shares.push_back(shares[i]);
            }
        }
        g_children.push_back(h_signal);
        g_shares.push_back(0);

        return split(*g, std::move(g_children), std::move(g_shares), used_fallback);
    };

    const mockturtle::topo_view topo{klut};

    topo.foreach_gate(
        [&](const auto& n)
        {
            std::vector<technology_network::signal> children{};
            std::vector<uint32_t>                   shares{};
            klut.foreach_fanin(n,
                               [&](const auto& f)
                               {
                                   children.push_back(old2new[klut.get_node(f)]);
                                   shares.push_back(users[key(depth.level(n), klut.get_node(f))] - 1u);
                               });

            auto f = klut.node_function(n);
            if (f.num_vars() > children.size())
            {
                f = kitty::shrink_to(f, static_cast<uint32_t>(children.size()));
            }

            if (children.size() <= TARGET_LUT_SIZE)
            {
                old2new[n] = dest.create_node(children, f);
                return;
            }

            bool used_fallback = false;
            old2new[n]         = split(f, children, shares, used_fallback);
            if (used_fallback)
            {
                ++result.fallbacks;
            }
            else
            {
                ++result.decomposed_luts;
            }
        });

    klut.foreach_po([&](const auto& po) { dest.create_po(old2new[klut.get_node(po)]); });

    return result;
}

/**
 * Result of an equivalence check.
 */
struct equivalence_result
{
    bool        equivalent{false};
    std::string method{"miter"};
};

/**
 * Simulates random patterns on both networks and compares the outputs.
 */
template <typename Spec, typename Impl>
bool simulate_equivalent(const Spec& spec, const Impl& impl)
{
    using tt_t = kitty::partial_truth_table;

    const mockturtle::partial_simulator sim{spec.num_pis(), SIMULATION_PATTERNS};

    return mockturtle::simulate<tt_t>(spec, sim) == mockturtle::simulate<tt_t>(impl, sim);
}

/**
 * Checks equivalence by SAT on the `virtual_miter` up to a conflict limit, by random simulation beyond it.
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

/**
 * Splits a comma-separated list.
 */
std::vector<std::string> split(const std::string_view s)
{
    std::vector<std::string> parts{};
    std::size_t              start = 0;

    while (start <= s.size())
    {
        const auto end = s.find(',', start);
        parts.emplace_back(s.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start));
        if (end == std::string_view::npos)
        {
            break;
        }
        start = end + 1;
    }

    return parts;
}

}  // namespace

int main(const int argc, const char** argv)  // NOLINT
{
    // options: --k 4,5,6,8   --strategy name,...   benchmark names such as ISCAS85/c17 restrict the run
    std::vector<uint32_t>    ks{4u, 5u, 6u, 8u};
    std::vector<std::string> wanted{};
    std::vector<std::string> benchmarks{};
    std::string              blif_dir{};

    const std::span<const char*> args{argv, static_cast<std::size_t>(argc)};

    for (std::size_t i = 1; i < args.size(); ++i)
    {
        const std::string_view arg{args[i]};

        if (arg == "--k" && i + 1 < args.size())
        {
            ks.clear();
            for (const auto& part : split(args[++i]))
            {
                ks.push_back(static_cast<uint32_t>(std::stoul(part)));
            }
        }
        else if (arg == "--strategy" && i + 1 < args.size())
        {
            wanted = split(args[++i]);
        }
        else if (arg == "--blif-dir" && i + 1 < args.size())
        {
            // ABC results: <dir>/<set>__<name>/<K>/<variant>.blif, evaluated instead of the mockturtle strategies
            blif_dir = args[++i];
        }
        else
        {
            benchmarks.emplace_back(arg);
        }
    }

    if (benchmarks.empty())
    {
        benchmarks = fiction_experiments::all_benchmarks(PAPER_BENCHMARKS);
    }

    auto all_strategies = strategies();

    if (!wanted.empty())
    {
        std::erase_if(all_strategies, [&wanted](const auto& s)
                      { return std::find(wanted.cbegin(), wanted.cend(), s.first) == wanted.cend(); });
    }

    const bool want_direct = wanted.empty() || std::find(wanted.cbegin(), wanted.cend(), "direct") != wanted.cend();
    const bool want_remap  = wanted.empty() || std::find(wanted.cbegin(), wanted.cend(), "remap") != wanted.cend();

    static mockturtle::shannon_resynthesis<mockturtle::xag_network> shannon_for_fallback{};
    const auto                                                      fallback = via_xag(
        mockturtle::dsd_resynthesis<mockturtle::xag_network, decltype(shannon_for_fallback)>{shannon_for_fallback});

    experiments::experiment<std::string, uint32_t, std::string, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t,
                            uint32_t, uint32_t, uint64_t, uint64_t, double, double, bool, std::string>
        survey{"decomposition_survey",
               "benchmark",
               "K",
               "strategy",
               "K-LUTs",
               "LUTs",
               "max fanin",
               "nodes",
               "depth",
               "decomposed",
               "fallbacks",
               "crossings unordered",
               "crossings",
               "t decomp [s]",
               "t mincross [s]",
               "equivalent",
               "check"};

    uint32_t failures = 0;

    // evaluates one result network: balance, order, count, check
    const auto evaluate = [&](const std::string& benchmark, const uint32_t k, const std::string& strategy,
                              const mockturtle::klut_network& klut, const technology_network& spec,
                              const decomposed& dec, const double t_decomp)
    {
        const auto balanced = network_balancing<technology_network>(dec.network);

        mutable_rank_view ranked{balanced};
        const auto        unordered = count_crossings(ranked);

        utils::graph::mincross_stats      mc_st{};
        mockturtle::stopwatch<>::duration t_mincross{};
        {
            const mockturtle::stopwatch stop{t_mincross};
            utils::graph::mincross(ranked, {}, &mc_st);
        }

        const auto eq = is_equivalent(spec, balanced);

        if (!eq.equivalent)
        {
            ++failures;
            fmt::print("[e] {} K={} {}: not equivalent\n", benchmark, k, strategy);
        }

        survey(benchmark, k, strategy, klut.num_gates(), num_luts(dec.network), max_fanin(dec.network),
               balanced.num_gates(), ranked.depth(), dec.decomposed_luts, dec.fallbacks, unordered, mc_st.num_crossings,
               t_decomp, mockturtle::to_seconds(t_mincross), eq.equivalent, eq.method);

        survey.save();
        survey.table();
    };

    for (const auto& benchmark : benchmarks)
    {
        fmt::print("[i] processing {}\n", benchmark);

        try
        {
            const auto aig = mockturtle::cleanup_dangling(read_aig(benchmark));

            if (!blif_dir.empty())
            {
                // every variant of every K in the benchmark's directory, checked against the AIG
                auto benchmark_dir = benchmark;
                std::replace(benchmark_dir.begin(), benchmark_dir.end(), '/', '_');
                benchmark_dir.insert(benchmark_dir.find('_'), "_");

                for (const auto k : ks)
                {
                    const std::filesystem::path dir =
                        std::filesystem::path{blif_dir} / benchmark_dir / std::to_string(k);

                    if (!std::filesystem::is_directory(dir))
                    {
                        continue;
                    }

                    std::vector<std::filesystem::path> files{};
                    for (const auto& entry : std::filesystem::directory_iterator{dir})
                    {
                        if (entry.path().extension() == ".blif")
                        {
                            files.push_back(entry.path());
                        }
                    }
                    std::ranges::sort(files);

                    for (const auto& file : files)
                    {
                        const auto name = file.stem().string();

                        if (!wanted.empty() && std::find(wanted.cbegin(), wanted.cend(), name) == wanted.cend())
                        {
                            continue;
                        }

                        decomposed dec{};
                        dec.network = read_blif(file.string());

                        if (dec.network.num_pis() != aig.num_pis() || dec.network.num_pos() != aig.num_pos())
                        {
                            ++failures;
                            fmt::print("[e] {} K={} {}: {} inputs and {} outputs, the AIG has {} and {}\n", benchmark,
                                       k, name, dec.network.num_pis(), dec.network.num_pos(), aig.num_pis(),
                                       aig.num_pos());
                            continue;
                        }

                        // the K-LUT column is unknown for ABC results; the AIG is the specification
                        evaluate(benchmark, k, name, mockturtle::klut_network{},
                                 convert_network<technology_network>(aig), dec, 0.0);
                    }
                }

                continue;
            }

            for (const auto k : ks)
            {
                const auto klut = map_to_luts(aig, k);
                const auto spec = convert_network<technology_network>(klut);

                fmt::print("[i]   K = {}: {} LUTs\n", k, klut.num_gates());

                // the K-LUT network must be equivalent to the AIG, otherwise nothing below means anything
                if (const auto m = mockturtle::miter<mockturtle::klut_network>(aig, klut); m.has_value())
                {
                    mockturtle::equivalence_checking_params eps{};
                    eps.conflict_limit = SAT_CONFLICT_LIMIT;
                    if (const auto r = mockturtle::equivalence_checking(*m, eps); r.has_value() && !*r)
                    {
                        throw std::runtime_error{fmt::format("the {}-LUT mapping is not equivalent to the AIG", k)};
                    }
                }

                if (k == TARGET_LUT_SIZE && want_direct)
                {
                    decomposed dec{};
                    dec.network = spec;
                    evaluate(benchmark, k, "direct", klut, spec, dec, 0.0);
                }

                if (k > TARGET_LUT_SIZE && want_remap)
                {
                    mockturtle::stopwatch<>::duration t{};
                    decomposed                        dec{};
                    {
                        const mockturtle::stopwatch stop{t};
                        const auto graph = mockturtle::convert_klut_to_graph<mockturtle::aig_network>(klut);
                        dec.network      = convert_network<technology_network>(map_to_luts(graph, TARGET_LUT_SIZE));
                    }
                    evaluate(benchmark, k, "remap", klut, spec, dec, mockturtle::to_seconds(t));
                }

                if (k > TARGET_LUT_SIZE)
                {
                    for (const auto& [name, rule] :
                         std::vector<std::pair<std::string, bound_set_rule>>{{"ash_shared", bound_set_rule::SHARED},
                                                                             {"ash_unshared", bound_set_rule::UNSHARED},
                                                                             {"ash_first", bound_set_rule::FIRST}})
                    {
                        if (!wanted.empty() && std::find(wanted.cbegin(), wanted.cend(), name) == wanted.cend())
                        {
                            continue;
                        }

                        mockturtle::stopwatch<>::duration t{};
                        decomposed                        dec{};
                        {
                            const mockturtle::stopwatch stop{t};
                            dec = decompose_ashenhurst(klut, rule, fallback);
                        }
                        evaluate(benchmark, k, name, klut, spec, dec, mockturtle::to_seconds(t));
                    }

                    for (const auto& [name, strategy] : all_strategies)
                    {
                        mockturtle::stopwatch<>::duration t{};
                        decomposed                        dec{};
                        {
                            const mockturtle::stopwatch stop{t};
                            dec = decompose(klut, strategy, fallback);
                        }
                        evaluate(benchmark, k, name, klut, spec, dec, mockturtle::to_seconds(t));
                    }
                }
            }
        }
        catch (const std::exception& e)
        {
            ++failures;
            fmt::print("[e] {}: {}\n", benchmark, e.what());
        }
    }

    fmt::print("[i] {} failures\n", failures);

    return failures == 0 ? 0 : 1;
}
