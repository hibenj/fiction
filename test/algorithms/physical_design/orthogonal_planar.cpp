//
// Created by benjamin on 7/29/24.
//
#include <catch2/catch_test_macros.hpp>

#include "fiction/algorithms/network_transformation/fanout_substitution.hpp"
#include "fiction/algorithms/network_transformation/network_balancing.hpp"
#include "fiction/algorithms/network_transformation/network_conversion.hpp"
#include "fiction/algorithms/network_transformation/node_duplication_planarization.hpp"
#include "fiction/networks/views/mutable_rank_view.hpp"
#include "fiction/utils/debug/network_writer.hpp"
#include "utils/blueprints/network_blueprints.hpp"
#include "utils/equivalence_checking_utils.hpp"

#include <fiction/algorithms/physical_design/apply_gate_library.hpp>
#include <fiction/algorithms/physical_design/orthogonal_planar.hpp>
#include <fiction/layouts/cartesian_layout.hpp>
#include <fiction/layouts/cell_level_layout.hpp>
#include <fiction/layouts/clocked_layout.hpp>
#include <fiction/layouts/gate_level_layout.hpp>
#include <fiction/layouts/tile_based_layout.hpp>
#include <fiction/networks/technology_network.hpp>
#include <fiction/networks/virtual_pi_network.hpp>
#include <fiction/technology/qca_one_library.hpp>

#include <mockturtle/networks/aig.hpp>
#include <mockturtle/networks/mig.hpp>
#include <mockturtle/views/fanout_view.hpp>
#include <mockturtle/views/names_view.hpp>
#include <mockturtle/views/rank_view.hpp>

#include <type_traits>
#include <vector>

using namespace fiction;

TEST_CASE("Case buf", "[orthogonal-planar]")
{
    using gate_layout = gate_level_layout<clocked_layout<tile_based_layout<cartesian_layout<offset::ucoord_t>>>>;

    virtual_pi_network<technology_network> v_ntk{};

    const auto x1 = v_ntk.create_pi();
    const auto x2 = v_ntk.create_pi();
    const auto x3 = v_ntk.create_virtual_pi(x1);
    const auto x4 = v_ntk.create_virtual_pi(x2);
    const auto x5 = v_ntk.create_pi();
    const auto x6 = v_ntk.create_pi();
    const auto x7 = v_ntk.create_virtual_pi(x5);
    const auto x8 = v_ntk.create_virtual_pi(x6);

    const auto f1 = v_ntk.create_and(x1, x2);
    const auto f2 = v_ntk.create_and(x3, x4);
    const auto f3 = v_ntk.create_and(x5, x6);
    const auto f4 = v_ntk.create_and(x7, x8);

    const auto f5 = v_ntk.create_and(f1, f2);
    const auto f6 = v_ntk.create_and(f3, f4);

    v_ntk.create_po(f5);
    v_ntk.create_po(f5);
    v_ntk.create_po(f5);
    v_ntk.create_po(f5);
    v_ntk.create_po(f5);
    v_ntk.create_po(f5);
    v_ntk.create_po(f5);
    v_ntk.create_po(f5);

    v_ntk.create_po(f6);
    v_ntk.create_po(f6);
    v_ntk.create_po(f6);
    v_ntk.create_po(f6);
    v_ntk.create_po(f6);
    v_ntk.create_po(f6);
    v_ntk.create_po(f6);
    v_ntk.create_po(f6);

    network_balancing_params ps;
    ps.unify_outputs = true;

    const auto fo_ntk = network_balancing<technology_network>(fanout_substitution<technology_network>(v_ntk), ps);

    const mutable_rank_view aig_r(fo_ntk);

    const std::vector<mockturtle::aig_network::node> nodes_rank0{1, 2, 3};
    const std::vector<mockturtle::aig_network::node> nodes_rank1{4, 5};

    /*aig_r.set_ranks(0, nodes_rank0);
    aig_r.set_ranks(0, nodes_rank0);*/

    const auto layout = orthogonal_planar<gate_layout>(aig_r);

    debug::write_dot_layout(layout);
    debug::write_dot_network(fo_ntk);
}

// New test case
TEST_CASE("Edge case", "[orthogonal-planar]")
{
    using gate_layout = gate_level_layout<clocked_layout<tile_based_layout<cartesian_layout<offset::ucoord_t>>>>;

    virtual_pi_network<technology_network> v_ntk{};

    const auto x1 = v_ntk.create_pi();
    const auto x2 = v_ntk.create_pi();
    const auto x3 = v_ntk.create_virtual_pi(x1);
    const auto x4 = v_ntk.create_virtual_pi(x2);

    const auto f1 = v_ntk.create_and(x1, x2);
    const auto n1 = v_ntk.create_not(x2);
    const auto n2 = v_ntk.create_not(x3);
    const auto f2 = v_ntk.create_and(n1, n2);
    const auto f3 = v_ntk.create_and(x3, x4);

    v_ntk.create_po(f1);
    v_ntk.create_po(f2);
    v_ntk.create_po(f3);

    network_balancing_params ps;
    ps.unify_outputs = true;

    const auto fo_ntk = network_balancing<technology_network>(fanout_substitution<technology_network>(v_ntk), ps);

    mutable_rank_view aig_r(fo_ntk);

    const std::vector<mockturtle::aig_network::node> nodes_rank1{7, 6, 10, 13};
    const std::vector<mockturtle::aig_network::node> nodes_rank2{8, 9, 11, 14};
    const std::vector<mockturtle::aig_network::node> nodes_rank3{15, 12, 16};

    aig_r.set_ranks(1, nodes_rank1);
    aig_r.set_ranks(2, nodes_rank2);
    aig_r.set_ranks(3, nodes_rank3);

    const auto layout = orthogonal_planar<gate_layout>(aig_r);

    debug::write_dot_layout(layout);
    debug::write_dot_network(fo_ntk);

    technology_network t{};
    const auto p1 = t.create_pi();
    const auto p2 = t.create_pi();
    const auto a1 = t.create_and(p1, p2);
    t.create_po(a1);

    auto fc         = fanins(t, a1);
    std::cout << "Size: " << t.size() << "\n";
    auto sub_signal = t.create_or(fc.fanin_nodes[0], fc.fanin_nodes[1]);
    t.substitute_node(a1, sub_signal);
    t = cleanup_dangling(t);
    std::cout << "Size: " << t.size() << "\n";
}

TEST_CASE("And gaps", "[orthogonal-planar]")
{
    using gate_layout = gate_level_layout<clocked_layout<tile_based_layout<cartesian_layout<offset::ucoord_t>>>>;

    virtual_pi_network<technology_network> v_ntk{};

    const auto x1 = v_ntk.create_pi();
    const auto x2 = v_ntk.create_pi();
    const auto x3 = v_ntk.create_virtual_pi(x1);
    const auto x4 = v_ntk.create_virtual_pi(x2);
    const auto x5 = v_ntk.create_pi();
    const auto x6 = v_ntk.create_pi();
    const auto x7 = v_ntk.create_virtual_pi(x5);

    const auto b1 = v_ntk.create_buf(x1);
    const auto a1 = v_ntk.create_and(x2, x3);
    const auto a2 = v_ntk.create_and(x4, x5);
    const auto a3 = v_ntk.create_and(x6, x7);

    const auto a4 = v_ntk.create_and(b1, a1);
    const auto a5 = v_ntk.create_and(a2, a3);

    v_ntk.create_po(a4);
    v_ntk.create_po(a5);

    network_balancing_params ps;
    ps.unify_outputs = true;

    const auto fo_ntk = network_balancing<technology_network>(fanout_substitution<technology_network>(v_ntk), ps);

    const mutable_rank_view aig_r(fo_ntk);

    const std::vector<mockturtle::aig_network::node> nodes_rank0{1, 2, 3};
    const std::vector<mockturtle::aig_network::node> nodes_rank1{4, 5};

    /*aig_r.set_ranks(0, nodes_rank0);
    aig_r.set_ranks(0, nodes_rank0);*/

    const auto layout = orthogonal_planar<gate_layout>(aig_r);

    debug::write_dot_layout(layout);
    debug::write_dot_network(fo_ntk);
}

TEST_CASE("Fo to And gaps", "[orthogonal-planar]")
{
    using gate_layout = gate_level_layout<clocked_layout<tile_based_layout<cartesian_layout<offset::ucoord_t>>>>;

    virtual_pi_network<technology_network> v_ntk{};

    const auto x1 = v_ntk.create_pi();
    const auto x2 = v_ntk.create_pi();
    const auto x3 = v_ntk.create_virtual_pi(x1);
    // const auto x4 = v_ntk.create_virtual_pi(x2);
    /*const auto x5 = v_ntk.create_pi();
    const auto x6 = v_ntk.create_pi();
    const auto x7 = v_ntk.create_virtual_pi(x5);
    const auto x8 = v_ntk.create_virtual_pi(x6);*/

    const auto b1 = v_ntk.create_buf(x1);
    const auto b2 = v_ntk.create_buf(x2);
    const auto b3 = v_ntk.create_buf(x3);
    const auto f1 = v_ntk.create_and(b2, b3);
    const auto b4 = v_ntk.create_buf(f1);

    v_ntk.create_po(b1);
    v_ntk.create_po(b1);
    v_ntk.create_po(b1);
    v_ntk.create_po(b1);
    v_ntk.create_po(b1);
    v_ntk.create_po(b1);
    v_ntk.create_po(b1);
    v_ntk.create_po(b1);

    v_ntk.create_po(b4);

    network_balancing_params ps;
    ps.unify_outputs = true;

    const auto fo_ntk = network_balancing<technology_network>(fanout_substitution<technology_network>(v_ntk), ps);

    const mutable_rank_view aig_r(fo_ntk);

    const std::vector<mockturtle::aig_network::node> nodes_rank0{1, 2, 3};
    const std::vector<mockturtle::aig_network::node> nodes_rank1{4, 5};

    /*aig_r.set_ranks(0, nodes_rank0);
    aig_r.set_ranks(0, nodes_rank0);*/

    const auto layout = orthogonal_planar<gate_layout>(aig_r);

    debug::write_dot_layout(layout);
    debug::write_dot_network(fo_ntk);
}

TEST_CASE("Test blueprints", "[orthogonal-planar]")
{
    using gate_layout = gate_level_layout<clocked_layout<tile_based_layout<cartesian_layout<offset::ucoord_t>>>>;

    auto maj = blueprints::and_or_network<mockturtle::names_view<mockturtle::aig_network>>();

    auto maj_t = convert_network<technology_network>(maj);

    network_balancing_params ps;
    ps.unify_outputs = true;

    const auto maj_tec_b = network_balancing<technology_network>(fanout_substitution<technology_network>(maj_t), ps);

    const auto maj_r = fiction::mutable_rank_view(maj_tec_b);

    auto planarized_maj = fiction::node_duplication_planarization(maj_r);

    const auto layout = orthogonal_planar<gate_layout>(planarized_maj);

    debug::write_dot_layout(layout);

    debug::write_dot_network(maj_tec_b);
}

TEST_CASE("Print layout", "[orthogonal-planar]")
{
    using gate_layout = gate_level_layout<clocked_layout<tile_based_layout<cartesian_layout<offset::ucoord_t>>>>;

    virtual_pi_network<technology_network> v_ntk{};

    const auto x1 = v_ntk.create_pi();
    const auto x2 = v_ntk.create_pi();
    const auto x3 = v_ntk.create_virtual_pi(x1);
    const auto x4 = v_ntk.create_virtual_pi(x2);

    const auto f1 = v_ntk.create_and(x1, x2);
    const auto f2 = v_ntk.create_and(x2, x3);
    const auto f3 = v_ntk.create_and(x3, x4);

    v_ntk.create_po(f1);
    v_ntk.create_po(f2);
    v_ntk.create_po(f3);

    network_balancing_params ps;
    ps.unify_outputs = true;

    const auto fo_ntk = network_balancing<technology_network>(fanout_substitution<technology_network>(v_ntk), ps);

    mutable_rank_view aig_r(fo_ntk);

    std::vector<mockturtle::aig_network::node> nodes_rank0{7, 6, 9, 11};
    std::vector<mockturtle::aig_network::node> nodes_rank1{8, 10, 12};

    aig_r.set_ranks(1, nodes_rank0);
    aig_r.set_ranks(2, nodes_rank1);

    const auto layout = orthogonal_planar<gate_layout>(aig_r);

    debug::write_dot_layout(layout);
    debug::write_dot_network(fo_ntk);
}

TEST_CASE("Print layout two", "[orthogonal-planar]")
{
    using gate_layout = gate_level_layout<clocked_layout<tile_based_layout<cartesian_layout<offset::ucoord_t>>>>;

    virtual_pi_network<technology_network> v_ntk{};

    const auto x1 = v_ntk.create_pi();
    const auto x2 = v_ntk.create_pi();
    const auto x3 = v_ntk.create_virtual_pi(x1);
    const auto x4 = v_ntk.create_virtual_pi(x2);

    const auto f1 = v_ntk.create_and(x1, x2);
    const auto f2 = v_ntk.create_and(x2, x3);
    const auto f3 = v_ntk.create_and(x3, x4);

    v_ntk.create_po(f1);
    v_ntk.create_po(f2);
    v_ntk.create_po(f3);

    network_balancing_params ps;
    ps.unify_outputs = true;

    const auto fo_ntk = network_balancing<technology_network>(fanout_substitution<technology_network>(v_ntk), ps);

    mutable_rank_view aig_r(fo_ntk);

    std::vector<mockturtle::aig_network::node> nodes_rank0{7, 6, 9, 11};
    std::vector<mockturtle::aig_network::node> nodes_rank1{8, 10, 12};

    aig_r.set_ranks(1, nodes_rank0);
    aig_r.set_ranks(2, nodes_rank1);

    const auto layout = orthogonal_planar<gate_layout>(aig_r);

    debug::write_dot_layout(layout);
    debug::write_dot_network(fo_ntk);
}

TEST_CASE("Backward Propagation", "[orthogonal-planar]")
{
    using gate_layout = gate_level_layout<clocked_layout<tile_based_layout<cartesian_layout<offset::ucoord_t>>>>;

    fiction::technology_network ntk;

    const auto pi0 = ntk.create_pi();
    const auto pi1 = ntk.create_pi();
    const auto pi2 = ntk.create_pi();
    const auto pi3 = ntk.create_pi();
    const auto pi4 = ntk.create_pi();
    const auto pi5 = ntk.create_pi();

    const auto n2 = ntk.create_not(pi2);
    const auto n3 = ntk.create_not(pi3);
    const auto a0 = ntk.create_and(pi0, pi1);
    const auto a1 = ntk.create_and(pi4, pi5);
    const auto a2 = ntk.create_and(a0, pi2);
    const auto a3 = ntk.create_and(n2, n3);
    const auto a4 = ntk.create_and(a1, pi3);
    const auto o0 = ntk.create_or(pi4, pi5);

    ntk.create_po(a2);
    ntk.create_po(a3);
    ntk.create_po(a4);
    ntk.create_po(o0);

    network_balancing_params ps;
    ps.unify_outputs = true;

    const auto fo_ntk = network_balancing<technology_network>(fanout_substitution<technology_network>(ntk), ps);

    const auto fo_ntk_r = fiction::mutable_rank_view(fo_ntk);

    auto planarized_ntk = node_duplication_planarization(fo_ntk_r);

    // planarized_ntk.update_ranks();

    const auto layout = orthogonal_planar<gate_layout>(planarized_ntk);

    debug::write_dot_layout(layout);
    debug::write_dot_network(planarized_ntk);
}
