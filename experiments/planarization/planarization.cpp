//
// Created by benjamin on 30.06.25.
//

#include "fiction/algorithms/graph/mincross.hpp"
#include "fiction/algorithms/network_transformation/inverter_substitution.hpp"
#include "fiction/algorithms/network_transformation/network_balancing.hpp"
#include "fiction/algorithms/network_transformation/node_duplication_planarization.hpp"
#include "fiction/algorithms/physical_design/apply_gate_library.hpp"
#include "fiction/algorithms/physical_design/graph_oriented_layout_design.hpp"  // graph-oriented layout design algorithm
#include "fiction/algorithms/physical_design/orthogonal.hpp"  // graph-oriented layout design algorithm
#include "fiction/algorithms/physical_design/orthogonal_planar.hpp"
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

    experiments::experiment<std::string, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint64_t,
                            uint64_t, bool>
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
    static constexpr const uint64_t bench_select = (fiction_experiments::c2670);

    for (const auto& benchmark : fiction_experiments::all_benchmarks(bench_select))
    {
        auto benchmark_network = mockturtle::cleanup_dangling(read_ntk<fiction::tec_nt>(benchmark));

        fiction::network_balancing_params b_ps;
        b_ps.unify_outputs = true;

        const auto _b = fiction::network_balancing<fiction::technology_network>(
            fiction::fanout_substitution<fiction::technology_network>(benchmark_network), b_ps);

        // fiction::debug::write_dot_network(_b, "balanced_ntk");

        if (_b.size() > 10000)
        {
            continue;
        }

        auto _r = fiction::mutable_rank_view(_b);

        fiction::mincross_stats        st{};
        fiction::mincross_params p{};
        p.fixed_pis = false;
        auto                           ntk          = mincross(_r, p, &st, false);  // counts crossings
        const auto                     cross_before = st.num_crossings;
        ntk                                         = mincross(_r, p, &st);
        const auto cross_after                      = st.num_crossings;

        // auto planarized_dup_b = fiction::node_duplication_planarization(_r);

        // fiction::debug::write_dot_network(planarized_dup_b, "planarized_ntk");

        bool eq = true;
        planarizaion_exp(benchmark, benchmark_network.num_pis(), benchmark_network.num_pos(),
                         benchmark_network.num_gates(), _b.num_gates(), cross_before, cross_after,
                         ntk.num_gates(), ntk.num_gates(), eq);

        planarizaion_exp.save();
        planarizaion_exp.table();
    }

    return EXIT_SUCCESS;
}