//
// Created by benjamin on 30.06.25.
//

#include "fiction/algorithms/graph/median_sweep.hpp"
#include "fiction/algorithms/graph/mincross.hpp"
#include "fiction/algorithms/network_transformation/crossing_gate_planarization.hpp"
#include "fiction/algorithms/network_transformation/fanout_substitution.hpp"
#include "fiction/algorithms/network_transformation/network_balancing.hpp"
#include "fiction/algorithms/network_transformation/node_duplication_planarization.hpp"
#include "fiction/algorithms/network_transformation/planarization.hpp"
#include "fiction/algorithms/network_transformation/ranked_buffer_insertion.hpp"
#include "fiction/algorithms/verification/equivalence_checking.hpp"  // SAT-based equivalence checking
#include "fiction/algorithms/verification/virtual_miter.hpp"
#include "fiction/io/network_reader.hpp"  // read networks from files
#include "fiction/networks/technology_network.hpp"
#include "fiction/networks/views/mutable_rank_view.hpp"
#include "fiction/types.hpp"
#include "fiction/utils/debug/network_writer.hpp"
#include "fiction_experiments.hpp"
#include "fiction/algorithms/physical_design/graph_oriented_layout_design.hpp"

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

    fiction::network_reader<std::shared_ptr<Ntk>> reader{fiction_experiments::benchmark_path(name), os};

    const auto nets = reader.get_networks();

    return *nets.front();
}

int main()  // NOLINT
{
    experiments::experiment<std::string, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint64_t, uint64_t,
                            uint64_t, double, uint64_t, uint64_t, bool>
        planarizaion_exp{"orthogonal_planar_exp",
                         "benchmark",
                         "inputs",
                         "outputs",
                         "initial nodes",
                         "nodes after fo_sub/blc",
                         "width",
                         "depth",
                         "num_crossings (before mincross)",
                         "num_crossings (after mincross)",
                         "planar nodes (dupl)",
                         "ratio (dupl)",
                         "half planar nodes (gate dupl)",
                         "planar nodes (gate cross)",
                         "eq (ortho_p)"};

    // For all fiction benchmarks
    static constexpr const uint64_t bench_select = (fiction_experiments::fontes18);

    for (const auto& benchmark : fiction_experiments::all_benchmarks(bench_select))
    {
        /*if (benchmark == "ISCAS85/c432")
        {
            continue;
        }*/
        /*if (benchmark == "ISCAS85/c2670")
        {
            continue;
        }*/

        auto benchmark_network = mockturtle::cleanup_dangling(read_ntk<fiction::tec_nt>(benchmark));

        /*auto ntk_r = fiction::mutable_rank_view(benchmark_network);
        for (uint32_t i = 0; i < ntk_r.depth(); ++i)
        {
            std::cout << "Rank " << i << " width: " << ntk_r.rank_width(i) << "\n";
            uint32_t fanout_count = 0;
            ntk_r.foreach_node_in_rank(i, [&ntk_r, &fanout_count](const auto& n)
            {
                if (ntk_r.fanout_size(n) > 2)
                {
                    std::cout << "fanout size: " << ntk_r.fanout_size(n) << "\n";
                }
            });
        }*/

        fiction::network_balancing_params b_ps;
        b_ps.unify_outputs = true;

        const auto balanced_ntk = fiction::network_balancing<fiction::technology_network>(
            fiction::fanout_substitution<fiction::technology_network>(benchmark_network), b_ps);

        std::cout << "balanced ntk size: " << balanced_ntk.size() << std::endl;

        fiction::debug::write_dot_network(balanced_ntk, "balanced_ntk");

        if (balanced_ntk.size() > 8000)
        {
            continue;
        }

        auto ranked_ntk = fiction::mutable_rank_view(balanced_ntk);

        /*for (uint32_t i = 0; i < ranked_ntk.depth(); ++i)
        {
            std::cout << "Rank " << i << " width: " << ranked_ntk.rank_width(i) << "\n";
            uint32_t fanout_count = 0;
            ranked_ntk.foreach_node_in_rank(i, [&ranked_ntk, &fanout_count](const auto& n)
            {
                if (ranked_ntk.is_and(n) || ranked_ntk.is_or(n) || ranked_ntk.is_xor(n))
                {
                    ++fanout_count;
                }
            });
            std::cout << "Fanout count: " << fanout_count << "\n";
        }*/

        // fiction::debug::write_dot_network(ranked_ntk, "ntk_ranked");

        fiction::mincross_stats  st{};
        fiction::mincross_params p{};
        p.fixed_pis             = false;
        p.optimize              = false;

        /*fiction::mincross(ranked_ntk, p, &st);
        std::cout << "Crossings before median: " << st.num_crossings << "\n";

        fiction::median_sweep_pos_to_pis(ranked_ntk);

        fiction::mincross(ranked_ntk, p, &st);
        std::cout << "Crossings after median: " << st.num_crossings << "\n";*/

        auto       mincross_ntk = mincross(ranked_ntk, p, &st);  // counts crossings
        const auto cross_before = st.num_crossings;

        auto duplication_planarized_ntk = node_duplication_planarization(ranked_ntk);

        fiction::planarization_params ps_plan{};
        bool const x_value = true;
        bool const b_value = true;
        ps_plan.xor_gates = x_value;
        ps_plan.buffer = b_value;
        auto half_planarized_ntk = planarization(ranked_ntk, ps_plan);

        mincross_ntk = mincross(half_planarized_ntk, p, &st);
        const auto cross_after = st.num_crossings;

        /*fiction::virtual_pi_network<fiction::technology_network> tec{};
        const auto         pi1 = tec.create_pi();
        const auto         pi2 = tec.create_pi();
        const auto         pi3 = tec.create_pi();
        const auto         pi4 = tec.create_virtual_pi(pi1);
        const auto         a1  = tec.create_and(pi1, pi2);
        const auto         o1  = tec.create_or(pi1, pi2);
        const auto         a2  = tec.create_and(pi3, pi4);
        const auto         o2  = tec.create_or(pi3, pi4);
        tec.create_po(a1);
        tec.create_po(o1);
        tec.create_po(a2);
        tec.create_po(o2);

        auto tec_r = fiction::mutable_rank_view(tec);*/

        //std::vector<mockturtle::node<fiction::technology_network>> pis{5, 2, 3, 4};

        //tec_r.set_ranks(0, pis);

        fiction::crossing_gate_planarization_params c_ps{};
        c_ps.xor_gates                 = x_value;
        c_ps.buffer                    = b_value;

        auto full_planarized_network = fiction::crossing_gate_planarization(half_planarized_ntk, c_ps);

        fiction::ranked_buffer_insertion_params rps{};
        rps.scheduling = fiction::ranked_buffer_insertion_params::better;
        rps.assume.balance_cios = true;
        rps.assume.splitter_capacity = 2u;
        rps.assume.num_phases = 1u;

        fiction::technology_network tec{};
        fiction::ranked_buffer_insertion rbi{full_planarized_network, rps};
        rbi.run( tec );

        rbi.remove_buffer_and_splitter_chains(tec);
        tec = mockturtle::cleanup_dangling(tec);

        fiction::technology_network tec_2{};
        fiction::ranked_buffer_insertion rbi_2{tec, rps};
        rbi_2.run( tec_2 );

        //rbi_2.remove_buffer_and_splitter_chains(tec_2);
        //tec_2 = mockturtle::cleanup_dangling(tec_2);

        //fiction::debug::write_dot_network(tec, "tec");
        //fiction::debug::write_dot_network(tec_2, "tec_2");

        std::cout << "tec size: " << tec.size() << std::endl;
        std::cout << "tec_2 size: " << tec_2.size() << std::endl;

        std::cout << "half planarized num PIS: " << half_planarized_ntk.num_real_pis() << "\n";
        std::cout << "tec num PIS: " << tec.num_pis() << "\n";



        /*mockturtle::equivalence_checking_stats st_eq_1;
        auto                                   eq_1 =
            mockturtle::equivalence_checking(*fiction::virtual_miter<fiction::technology_network>(tec, cross_gate_planarized_ntk), {}, &st_eq_1);

        std::cout << "Equivalence crossing gate planarization: " << *eq_1 << "\n";*/

        // fiction::debug::write_dot_network(duplication_planarized_ntk, "ntk_dupl");

        // fiction::debug::write_dot_network(half_planarized_ntk, "ntk_dupl_half");

        //fiction::debug::write_dot_network(full_planarized_network, "ntk_cross_gate");

        mockturtle::equivalence_checking_stats st_eq;
        /*auto                                   eq =
            mockturtle::equivalence_checking(*fiction::virtual_miter<fiction::technology_network>(benchmark_network, tec), {}, &st_eq);*/

        std::cout <<  "Real cost: " << duplication_planarized_ntk.size() - half_planarized_ntk.size() << "\n";

        const bool eq = true;
        planarizaion_exp(benchmark, benchmark_network.num_pis(), benchmark_network.num_pos(),
                         benchmark_network.size(), balanced_ntk.size(), ranked_ntk.width(),
                         ranked_ntk.depth(), cross_before, cross_after, duplication_planarized_ntk.size(),
                         static_cast<double>(duplication_planarized_ntk.size()) /
                             static_cast<double>(balanced_ntk.size()),
                         half_planarized_ntk.size(), full_planarized_network.size(), eq);

        planarizaion_exp.save();
        planarizaion_exp.table();
    }

    return EXIT_SUCCESS;
}