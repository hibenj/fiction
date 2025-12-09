//
// Created by benjamin on 30.06.25.
//

#include "fiction/algorithms/graph/mincross.hpp"
#include "fiction/algorithms/network_transformation/crossing_gate_planarization.hpp"
#include "fiction/algorithms/network_transformation/fanout_substitution.hpp"
#include "fiction/algorithms/network_transformation/network_balancing.hpp"
#include "fiction/algorithms/network_transformation/node_duplication_planarization.hpp"
#include "fiction/algorithms/verification/equivalence_checking.hpp"  // SAT-based equivalence checking
#include "fiction/algorithms/verification/virtual_miter.hpp"
#include "fiction/io/network_reader.hpp"  // read networks from files
#include "fiction/networks/technology_network.hpp"
#include "fiction/networks/views/mutable_rank_view.hpp"
#include "fiction/types.hpp"
#include "fiction/utils/debug/network_writer.hpp"
#include "fiction_experiments.hpp"

#include <fmt/core.h>
#include <mockturtle/algorithms/cleanup.hpp>
#include <mockturtle/utils/stopwatch.hpp>

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <stdio.h>

template <typename Ntk1, typename Ntk2>
inline bool abc_cec_two_ntk(Ntk1 const& ntk1, Ntk2 const& ntk2)
{
    mockturtle::write_bench(ntk1, "/tmp/test1.bench");
    mockturtle::write_bench(ntk2, "/tmp/test2.bench");
    std::string const command = fmt::format("abc -q \"cec -n /tmp/test1.bench /tmp/test2.bench\"");

    std::array<char, 128> buffer{};
    std::string           result;
#if WIN32
    std::unique_ptr<FILE, decltype(&_pclose)> pipe(_popen(command.c_str(), "r"), _pclose);
#else
    std::unique_ptr<FILE, decltype(&pclose)> const pipe(popen(command.c_str(), "r"), pclose);
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
    experiments::experiment<std::string, uint32_t, uint32_t, uint32_t, uint32_t, uint64_t, uint64_t, uint64_t, uint64_t,
                            bool>
        planarizaion_exp{"orthogonal_planar_exp",
                         "benchmark",
                         "inputs",
                         "outputs",
                         "initial nodes",
                         "nodes after fo_sub/blc",
                         "num_crossings (before mincross)",
                         "num_crossings (after mincross)",
                         "planar nodes (dupl)",
                         "planar nodes (gate cross)",
                         "eq (ortho_p)"};

    // For all fiction benchmarks
    static constexpr const uint64_t bench_select = (fiction_experiments::b1_r2);

    for (const auto& benchmark : fiction_experiments::all_benchmarks(bench_select))
    {
        auto benchmark_network = mockturtle::cleanup_dangling(read_ntk<fiction::tec_nt>(benchmark));

        fiction::network_balancing_params b_ps;
        b_ps.unify_outputs = true;

        const auto balanced_ntk = fiction::network_balancing<fiction::technology_network>(
            fiction::fanout_substitution<fiction::technology_network>(benchmark_network), b_ps);

        // fiction::debug::write_dot_network(_b, "balanced_ntk");

        if (balanced_ntk.size() > 10000)
        {
            continue;
        }

        auto ranked_ntk = fiction::mutable_rank_view(balanced_ntk);

        fiction::debug::write_dot_network(ranked_ntk, "ntk_ranked");

        fiction::mincross_stats  st{};
        fiction::mincross_params p{};
        p.fixed_pis             = false;
        p.optimize              = false;
        auto       mincross_ntk = mincross(ranked_ntk, p, &st);  // counts crossings
        const auto cross_before = st.num_crossings;
        p.optimize              = true;
        mincross_ntk            = mincross(ranked_ntk, p, &st);
        const auto cross_after  = st.num_crossings;

        auto duplication_planarized_ntk = node_duplication_planarization(mincross_ntk);

        auto cross_gate_planarized_ntk = crossing_gate_planarization(mincross_ntk);

        fiction::debug::write_dot_network(duplication_planarized_ntk, "ntk_dupl");

        fiction::debug::write_dot_network(cross_gate_planarized_ntk, "ntk_cross_gate");

        const bool eq = true;
        planarizaion_exp(benchmark, benchmark_network.num_pis(), benchmark_network.num_pos(),
                         benchmark_network.num_gates(), balanced_ntk.num_gates(), cross_before, cross_after,
                         duplication_planarized_ntk.num_gates(), cross_gate_planarized_ntk.num_gates(), eq);

        planarizaion_exp.save();
        planarizaion_exp.table();
    }

    return EXIT_SUCCESS;
}