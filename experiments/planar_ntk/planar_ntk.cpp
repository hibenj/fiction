//
// Created by benjamin on 30.06.25.
//

#include "fiction/algorithms/graph/median_sweep.hpp"
#include "fiction/algorithms/graph/mincross.hpp"
#include "fiction/algorithms/network_transformation/crossing_gate_planarization.hpp"
#include "fiction/algorithms/network_transformation/fanout_substitution.hpp"
#include "fiction/algorithms/network_transformation/network_balancing.hpp"
#include "fiction/algorithms/network_transformation/node_duplication_planarization.hpp"
#include "fiction/algorithms/network_transformation/node_duplication_planarization_f.hpp"
#include "fiction/algorithms/network_transformation/planarization.hpp"
#include "fiction/algorithms/network_transformation/ranked_buffer_insertion.hpp"
#include "fiction/algorithms/network_transformation/remove_buffer.hpp"
#include "fiction/algorithms/network_transformation/planar_fanout_substitution.hpp"
#include "fiction/algorithms/physical_design/graph_oriented_layout_design.hpp"
#include "fiction/algorithms/physical_design/planar_layout_from_network_embedding.hpp"
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
#include <mockturtle/algorithms/equivalence_checking.hpp>
#include <mockturtle/utils/stopwatch.hpp>
#include "mockturtle/views/rank_view.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

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

std::vector<std::string> collect_iwls93_benchmarks()
{
    namespace fs = std::filesystem;

    std::vector<std::string> benchmarks{};
    const auto iwls93_dir = fs::path{EXPERIMENTS_PATH} / "../benchmarks/IWLS93";

    if (!fs::exists(iwls93_dir) || !fs::is_directory(iwls93_dir))
    {
        fmt::print("[w] IWLS93 directory not found: {}\n", iwls93_dir.string());
        return benchmarks;
    }

    for (const auto& entry : fs::directory_iterator(iwls93_dir))
    {
        if (!entry.is_regular_file())
        {
            continue;
        }

        const auto& path = entry.path();
        if (path.extension() == ".v")
        {
            benchmarks.emplace_back(fmt::format("IWLS93/{}", path.stem().string()));
        }
    }

    std::sort(benchmarks.begin(), benchmarks.end());
    return benchmarks;
}

int main()  // NOLINT
{
    experiments::experiment<std::string, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint64_t, uint64_t,
                            uint64_t, double, uint64_t, uint64_t, double, bool>
        planarizaion_exp{"orthogonal_planar_exp",
                         "benchmark",
                         "inputs",
                         "outputs",
                         "initial nodes",
                         "nodes after blc",
                         "width",
                         "depth",
                         "num_crossings (before mincross)",
                         "num_crossings (after mincross)",
                         "planar nodes (dupl)",
                         "ratio (dupl)",
                         "half planar nodes",
                         "planar nodes",
                         "size decrease (%)",
                         "eq (ortho_p)"};

    experiments::experiment<std::string, std::string, uint32_t, uint32_t, uint64_t, bool, uint32_t, uint32_t, uint64_t,
                            double, std::string, std::string>
        placement_routing_exp{"orthogonal_planar_pr",
                              "benchmark",
                              "pr_algorithm",
                              "old width",
                              "old height",
                              "old area",
                              "old success",
                              "new width",
                              "new height",
                              "new area",
                              "area decrease old->new (%)",
                              "new eq",
                              "old eq"};

    // For all fiction benchmarks
    static constexpr uint64_t bench_select = fiction_experiments::trindade16;  // xor5_maj

    enum class benchmark_source
    {
        fiction_only,
        iwls93_only,
        combined
    };

    static constexpr benchmark_source selected_benchmark_source = benchmark_source::iwls93_only;

    std::vector<std::string> benchmarks_to_run{};
    if (selected_benchmark_source == benchmark_source::fiction_only ||
        selected_benchmark_source == benchmark_source::combined)
    {
        benchmarks_to_run = fiction_experiments::all_benchmarks(bench_select);
    }
    if (selected_benchmark_source == benchmark_source::iwls93_only ||
        selected_benchmark_source == benchmark_source::combined)
    {
        auto iwls93_benchmarks = collect_iwls93_benchmarks();
        benchmarks_to_run.insert(benchmarks_to_run.end(), iwls93_benchmarks.begin(), iwls93_benchmarks.end());
    }

    fmt::print("[i] benchmark source mode: {}\n",
               selected_benchmark_source == benchmark_source::iwls93_only ? "iwls93_only" :
               selected_benchmark_source == benchmark_source::fiction_only ? "fiction_only" :
                                                                             "combined");

    for (const auto& benchmark : benchmarks_to_run)
    {
        if (benchmark == "ISCAS85/c432")
        {
            continue;
        }
        // IWLS93/term1
        // IWLS93/C432
        if (benchmark != "IWLS93/term1")
        {
            continue;
        }

        auto benchmark_network = mockturtle::cleanup_dangling(read_ntk<fiction::tec_nt>(benchmark));

        // fiction::debug::write_dot_network(benchmark_network, "benchmark_network");

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

        /*##############################################################################################################
         * ###################################### Balancing and FO Subsitution #########################################
         * #############################################################################################################
         */

        fiction::network_balancing_params b_ps;
        b_ps.unify_outputs = true;

        const auto balanced_ntk = fiction::network_balancing<fiction::technology_network>(
            fiction::fanout_substitution<fiction::technology_network>(benchmark_network), b_ps);

        const auto b_ntk = fiction::network_balancing<fiction::technology_network>(benchmark_network, b_ps);

        // std::cout << "balanced ntk size: " << balanced_ntk.size() << std::endl;

        // fiction::debug::write_dot_network(b_ntk, "balanced_ntk");

        if (balanced_ntk.size() > 8000)
        {
            std::cout << "Skipping " << benchmark << "with size: " << balanced_ntk.size() << "\n";
            continue;
        }

        auto ranked_ntk = fiction::mutable_rank_view(balanced_ntk);

        auto r_ntk = fiction::mutable_rank_view(b_ntk);

        // fiction::debug::write_dot_network(ranked_ntk, "ntk_ranked");

        /*##############################################################################################################
         * ###################################### Mincross #############################################################
         * #############################################################################################################
         */

        fiction::mincross_stats  st{};
        fiction::mincross_params p{};
        p.fixed_pis = false;
        p.optimize  = false;

        auto       mincross_ntk = mincross(r_ntk, p, &st);  // counts crossings
        const auto cross_before = st.num_crossings;

        /*##############################################################################################################
         * ###################################### Network planarization ###############################################
         * #############################################################################################################
         */

        // Baseline 1
        /*std::cout << "Planarization with node duplication...\n";
        auto duplication_planarized_ntk = node_duplication_planarization(ranked_ntk);
        std::cout << "Size: " << duplication_planarized_ntk.size() << "\n";*/
        // Baseline 2
        std::cout << "Planarization with node duplication_f...\n";
        auto duplication_planarized_ntk_f = node_duplication_planarization_f(r_ntk);
        std::cout << "Size: " << duplication_planarized_ntk_f.size() << "\n";

        /*uint32_t num_buffers = 0;
        duplication_planarized_ntk.foreach_node([&duplication_planarized_ntk, &num_buffers](const auto& n)
        {
            if (duplication_planarized_ntk.is_buf(n))
            {
                ++num_buffers;
            }
        });

        std::cout << "Number of buffers in first duplication: " << num_buffers << "\n";
        uint32_t num_buffers_f = 0;
        std::cout << "Size for second duplication: " << duplication_planarized_ntk_f.size() << "\n";
        duplication_planarized_ntk_f.foreach_node([&duplication_planarized_ntk_f, &num_buffers_f](const auto& n)
        {
            if (duplication_planarized_ntk_f.is_buf(n))
            {
                ++num_buffers_f;
            }
        });
        std::cout << "Number of buffers in second duplication: " << num_buffers_f << "\n";
        std::cout << "Size Comparison: " << duplication_planarized_ntk.size() - num_buffers
                  << " to: " << duplication_planarized_ntk_f.size() - num_buffers_f << "\n";*/


        fiction::planarization_params ps_plan{};
        bool const                    x_value = true;
        bool const                    b_value = true;
        ps_plan.xor_gates                     = x_value;
        ps_plan.buffer                        = b_value;
        // Baseline 1
        /*std::cout << "Half planarization with planarization...\n";
        auto half_planarized_ntk              = planarization(ranked_ntk, ps_plan);
        std::cout << "Size: " << half_planarized_ntk.size() << "\n";*/
        fiction::node_duplication_planarization_f_params ps_f{};
        ps_f.xor_gates = x_value;
        ps_f.buffer    = b_value;
        ps_f.cross_gates = true;
        std::cout << "Half planarization with node_duplication_planarization_f...\n";
        auto half_planarized_ntk_f = node_duplication_planarization_f(r_ntk, ps_f);
        std::cout << "Size: " << half_planarized_ntk_f.size() << "\n";

        mincross(half_planarized_ntk_f, p, &st);
        const auto cross_after = st.num_crossings;

        fiction::crossing_gate_planarization_params c_ps{};
        c_ps.xor_gates = x_value;
        c_ps.buffer    = b_value;
        c_ps.verbose = false;

        /*std::vector<mockturtle::node<fiction::technology_network>> rank_1{21, 16, 43, 11, 12, 8, 7, 39, 40, 48, 64, 34, 68, 65, 31, 35, 30};
        rank_1 = {16, 43, 11, 12, 8, 7, 21, 39, 40, 48, 64, 34, 68, 65, 31, 35, 30};
        r_ntk.set_ranks(1, rank_1);*/

        // Baseline 1
        /*std::cout << "crossing_gate_planarization for half_planarized_ntk...\n";
        auto full_planarized_network = fiction::crossing_gate_planarization(half_planarized_ntk, c_ps);
        std::cout << "Size: " << full_planarized_network.size() << "\n";*/
        // Baseline 2
        std::cout << "crossing_gate_planarization for half_planarized_ntk_f...\n";
        auto full_planarized_network_f = fiction::crossing_gate_planarization(half_planarized_ntk_f, c_ps);
        std::cout << "Size: " << full_planarized_network_f.size() << "\n";

        /*##############################################################################################################
         * ###################################### AQFP Buffering ###############################################
         * #############################################################################################################
         */

        /*fiction::technology_network tec{};
        const auto pi1 = tec.create_pi();
        const auto pi2 = tec.create_pi();
        const auto buf1 = tec.create_buf(pi1);
        const auto inv1 = tec.create_not(buf1);
        const auto inv2 = tec.create_not(pi2);

        tec.create_po(inv1);
        tec.create_po(inv2);

        const auto tec_r = fiction::mutable_rank_view(tec);*/

        // fiction::debug::write_dot_network(full_planarized_rb, "full_planarized_rb");
        // fiction::debug::write_dot_network(final_ntk, "final_ntk");

        /*fiction::technology_network tec{};
        const auto pi1 = tec.create_pi();
        const auto inv1 = tec.create_not(pi1);
        const auto inv2 = tec.create_not(pi1);
        const auto inv3 = tec.create_not(pi1);
        const auto inv4 = tec.create_not(pi1);
        const auto inv5 = tec.create_not(pi1);
        tec.create_po(inv1);
        tec.create_po(inv2);
        tec.create_po(inv3);
        tec.create_po(inv4);
        tec.create_po(inv5);
        const auto tec_r = fiction::mutable_rank_view(tec);*/

        std::cout << "Start planar_fanout_substitution...\n";
        auto full_planarized_fanout = fiction::planar_fanout_substitution(full_planarized_network_f);
        std::cout << "Size: " << full_planarized_fanout.size() << "\n";
        // fiction::debug::write_dot_network(full_planarized_fanout, "full_planarized_fanout");

        fiction::remove_buffer_params ps_rb{};
        ps_rb.planar = true;
        ps_rb.insert_again = true;
        ps_rb.allowed_buff_fanout = 1u;
        std::cout << "Start removing buffers from full_planarized_network...\n";
        auto full_planarized_rb = fiction::remove_buffer(full_planarized_fanout, ps_rb);
        std::cout << "Size: " << full_planarized_rb.size() << "\n";

        std::cout << "Is Balanced: " << is_balanced(full_planarized_rb) << "\n";
        std::cout << "Is fanout substituted: " << is_fanout_substituted(full_planarized_rb) << "\n";
        // fiction::debug::write_dot_network(full_planarized_rb, "full_planarized_rb");
        mincross(full_planarized_rb, p, &st);
        std::cout << "Num Crossings: " <<  st.num_crossings << "\n";

        std::cout << "Start planar_fanout_substitution2...\n";
        auto full_planarized_fanout2 = fiction::planar_fanout_substitution(duplication_planarized_ntk_f);
        std::cout << "Size: " << full_planarized_fanout2.size() << "\n";

        std::cout << "Start removing buffers from full_planarized_network...\n";
        auto full_planarized_rb2 = fiction::remove_buffer(full_planarized_fanout2, ps_rb);
        std::cout << "Size: " << full_planarized_rb2.size() << "\n";

        std::cout << "Is Balanced: " << is_balanced(full_planarized_rb2) << "\n";
        std::cout << "Is fanout substituted: " << is_fanout_substituted(full_planarized_rb2) << "\n";
        mincross(full_planarized_rb2, p, &st);
        std::cout << "Num Crossings: " <<  st.num_crossings << "\n";

        /*fiction::ranked_buffer_insertion_params rps{};
        rps.scheduling = fiction::ranked_buffer_insertion_params::better;
        rps.assume.balance_cios = true;
        rps.assume.splitter_capacity = 2u;
        rps.assume.num_phases = 1u;
        rps.planar = false;

        std::cout << "Start aqfp buffering...\n";
        fiction::mutable_rank_view<fiction::technology_network> tec{};
        fiction::ranked_buffer_insertion rbi{full_planarized_rb, rps};
        rbi.run( tec );
        std::cout << "Size with buffers and fanouts aqfp: " << tec.size() << "\n";

        // fiction::debug::write_dot_network(tec, "tec");

        mincross(tec, p, &st);
        std::cout << "num_crossings after aqfp_bufering: " <<  st.num_crossings << "\n";*/

        /*fiction::mutable_rank_view<fiction::technology_network> tec_2{};
        fiction::ranked_buffer_insertion rbi_2{tec, rps};
        rbi_2.run( tec_2 );
        tec_2.update_ranks();

        std::cout << "Size with buffers and fanouts after removal: " << tec_2.size() << "\n";

        mincross(tec_2, p, &st);
        std::cout << "num_crossings after aqfp_bufering: " <<  st.num_crossings << "\n";*/

        /*rbi_2.remove_buffer_chains(tec_2);
        tec_2 = mockturtle::cleanup_dangling(tec_2);

        std::cout << "tec size: " << tec.size() << std::endl;
        std::cout << "tec_2 size: " << tec_2.size() << std::endl;

        std::cout << "half planarized num PIS: " << full_planarized_network.num_real_pis() << "\n";
        std::cout << "tec num PIS: " << tec.num_pis() << "\n";*/

        /*fiction::technology_network tec_2{};
        fiction::ranked_buffer_insertion rbi_2{tec, rps};
        rbi_2.run( tec_2 );

        std::cout << "tec size: " << tec.size() << std::endl;
        std::cout << "tec_2 size: " << tec_2.size() << std::endl;

        std::cout << "half planarized num PIS: " << half_planarized_ntk.num_real_pis() << "\n";
        std::cout << "tec num PIS: " << tec.num_pis() << "\n";*/

        // rbi_2.remove_buffer_and_splitter_chains(tec_2);
        // tec_2 = mockturtle::cleanup_dangling(tec_2);

        // fiction::debug::write_dot_network(tec_2, "tec_2");

        /*mockturtle::equivalence_checking_stats st_eq_1;
        auto                                   eq_1 =
            mockturtle::equivalence_checking(*fiction::virtual_miter<fiction::technology_network>(tec,
        cross_gate_planarized_ntk), {}, &st_eq_1);

        std::cout << "Equivalence crossing gate planarization: " << *eq_1 << "\n";*/

        // fiction::debug::write_dot_network(duplication_planarized_ntk, "duplication_planarized_ntk");

        // fiction::debug::write_dot_network(duplication_planarized_ntk_f, "duplication_planarized_ntk_f");

        // fiction::debug::write_dot_network(full_planarized_network_f, "full_planarized_network_f");
        // fiction::debug::write_dot_network(full_planarized_network, "full_planarized_network");

        mockturtle::equivalence_checking_stats st_eq;
        auto                                   eq = mockturtle::equivalence_checking(
            *fiction::virtual_miter<fiction::technology_network>(full_planarized_rb, benchmark_network), {},
            &st_eq);

        // const bool eq = true;
        const double size_decrease_dupl_to_gate_cross_percent =
            (full_planarized_rb2.size() > 0u) ?
                (100.0 *
                 (static_cast<double>(full_planarized_rb2.size()) - static_cast<double>(full_planarized_rb.size())) /
                 static_cast<double>(full_planarized_rb2.size())) :
                0.0;

        planarizaion_exp(
            benchmark, benchmark_network.num_pis(), benchmark_network.num_pos(), benchmark_network.size(),
            balanced_ntk.size(), r_ntk.width(), r_ntk.depth(), cross_before, cross_after,
            full_planarized_rb2.size(),
            static_cast<double>(full_planarized_rb2.size()) / static_cast<double>(r_ntk.size()),
            half_planarized_ntk_f.size(), full_planarized_rb.size(), size_decrease_dupl_to_gate_cross_percent, *eq);

        planarizaion_exp.save();
        planarizaion_exp.table();

        /*##############################################################################################################
         * ###################################### Placement and Routing ###############################################
         * #############################################################################################################
         */
        std::cout << "Starting placement and routing...\n";

        constexpr uint64_t max_pr_nodes = 30000ull;

        const bool old_fanout_ok = fiction::is_fanout_substituted(full_planarized_rb2);
        const bool new_fanout_ok = fiction::is_fanout_substituted(full_planarized_rb);
        const bool old_balance_ok = fiction::is_balanced(full_planarized_rb2);
        const bool new_balance_ok = fiction::is_balanced(full_planarized_rb);
        const bool old_size_ok = full_planarized_rb2.size() <= max_pr_nodes;
        const bool new_size_ok = full_planarized_rb.size() <= max_pr_nodes;

        const bool old_pr_eligible = old_fanout_ok && old_balance_ok && old_size_ok;
        const bool new_pr_eligible = new_fanout_ok && new_balance_ok && new_size_ok;

        if (!old_pr_eligible)
        {
            fmt::print("[w] skip old PR for {} (fanout_ok={}, balanced={}, size={} <= {})\n", benchmark, old_fanout_ok,
                       old_balance_ok, full_planarized_rb2.size(), max_pr_nodes);
        }
        if (!new_pr_eligible)
        {
            fmt::print("[w] skip new PR for {} (fanout_ok={}, balanced={}, size={} <= {})\n", benchmark, new_fanout_ok,
                       new_balance_ok, full_planarized_rb.size(), max_pr_nodes);
        }

        using gate_layout = fiction::gate_level_layout<
            fiction::clocked_layout<fiction::tile_based_layout<fiction::cartesian_layout<fiction::offset::ucoord_t>>>>;

        // Choose which placement & routing algorithm to run:
        // - graph_oriented_layout_design: works without an explicit planar embedding (can fail -> std::optional)
        // - plane: uses planar embedding-based design (may throw / may fail for non-planar nets)
        bool const use_plane_pr = true;

        // Common helper to turn a layout into width/height/area.
        const auto dims_from_layout = [](const gate_layout& lyt)
        {
            const auto bb     = fiction::bounding_box_2d(lyt);
            const auto width  = static_cast<uint32_t>(bb.get_x_size() + 1);
            const auto height = static_cast<uint32_t>(bb.get_y_size() + 1);
            const auto area   = static_cast<uint64_t>(width) * static_cast<uint64_t>(height);
            return std::tuple<uint32_t, uint32_t, uint64_t>{width, height, area};
        };

        uint32_t width_old_pr = 0u, height_old_pr = 0u;
        uint64_t area_old_pr    = 0ull;
        bool     success_old_pr = false;

        uint32_t width_new_pr = 0u, height_new_pr = 0u;
        uint64_t area_new_pr    = 0ull;
        bool     success_new_pr = false;

        // Track % area decrease (positive means new area is smaller). If old run failed or old area is 0, store 0.
        double area_decrease_old_to_new_percent = 0.0;

        std::string pr_algo_name;

        std::string eq_result{"N/A"};
        std::string eq_result2{"N/A"};

        if (use_plane_pr)
        {
            pr_algo_name = "plane";

            if (!new_pr_eligible)
            {
                eq_result = "SKIPPED";
            }
            else
            {
                try
                {
                    const auto layout_plane_new = fiction::plane<gate_layout>(full_planarized_rb);
                    std::tie(width_new_pr, height_new_pr, area_new_pr) = dims_from_layout(layout_plane_new);

                    const auto eq_pr = fiction::equivalence_checking(full_planarized_rb, layout_plane_new);
                    eq_result        = eq_pr == fiction::eq_type::STRONG ? "STRONG" :
                                       eq_pr == fiction::eq_type::WEAK   ? "WEAK" :
                                                                           "NO";

                    success_new_pr   = true;
                }
                catch (const std::exception& e)
                {
                    eq_result = "FAILED";
                    fmt::print("[w] plane PR failed (new) for {}: {}\n", benchmark, e.what());
                }
            }

            if (!old_pr_eligible)
            {
                eq_result2 = "SKIPPED";
            }
            else
            {
                try
                {
                    const auto layout_plane_old = fiction::plane<gate_layout>(full_planarized_rb2);
                    std::tie(width_old_pr, height_old_pr, area_old_pr) = dims_from_layout(layout_plane_old);

                    const auto eq_pr2 = fiction::equivalence_checking(full_planarized_rb2, layout_plane_old);
                    eq_result2        = eq_pr2 == fiction::eq_type::STRONG ? "STRONG" :
                                        eq_pr2 == fiction::eq_type::WEAK   ? "WEAK" :
                                                                             "NO";
                    success_old_pr    = true;
                }
                catch (const std::exception& e)
                {
                    eq_result2 = "FAILED";
                    fmt::print("[w] plane PR failed (old) for {}: {}\n", benchmark, e.what());
                }
            }

            if (success_old_pr && success_new_pr && area_old_pr > 0ull)
            {
                area_decrease_old_to_new_percent =
                    100.0 * (static_cast<double>(area_old_pr) - static_cast<double>(area_new_pr)) /
                    static_cast<double>(area_old_pr);
            }
        }
        else
        {
            pr_algo_name = "graph_oriented";

            fiction::graph_oriented_layout_design_stats  stats{};
            fiction::graph_oriented_layout_design_params params{};
            params.timeout      = 30000;
            params.return_first = true;
            params.planar       = true;

            if (old_pr_eligible)
            {
                const auto layout_old = fiction::graph_oriented_layout_design<gate_layout>(full_planarized_rb2, params, &stats);
                std::cout << "Crossing number old: " << stats.num_crossings << std::endl;
                if (layout_old)
                {
                    std::tie(width_old_pr, height_old_pr, area_old_pr) = dims_from_layout(*layout_old);
                    success_old_pr                                     = true;
                }
            }

            if (new_pr_eligible)
            {
                const auto layout_new = fiction::graph_oriented_layout_design<gate_layout>(full_planarized_rb, params, &stats);
                std::cout << "Crossing number new: " << stats.num_crossings << std::endl;
                if (layout_new)
                {
                    std::tie(width_new_pr, height_new_pr, area_new_pr) = dims_from_layout(*layout_new);
                    success_new_pr                                     = true;
                }
            }

            if (success_old_pr && success_new_pr && area_old_pr > 0ull)
            {
                area_decrease_old_to_new_percent =
                    100.0 * (static_cast<double>(area_old_pr) - static_cast<double>(area_new_pr)) /
                    static_cast<double>(area_old_pr);
            }
        }

        placement_routing_exp(benchmark, pr_algo_name, width_old_pr, height_old_pr, area_old_pr, success_old_pr,
                              width_new_pr, height_new_pr, area_new_pr, area_decrease_old_to_new_percent, eq_result, eq_result2);
        placement_routing_exp.save();
        placement_routing_exp.table();
    }

    return EXIT_SUCCESS;
}
