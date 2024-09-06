//
// Created by benjamin on 7/29/24.
//
#include <catch2/catch_test_macros.hpp>

#include "fiction/algorithms/network_transformation/fanout_substitution.hpp"
#include "fiction/algorithms/network_transformation/network_balancing.hpp"
#include "fiction/algorithms/network_transformation/node_duplication_planarization.hpp"
#include "fiction/networks/views/extended_rank_view.hpp"
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

    extended_rank_view aig_r(fo_ntk);

    std::vector<mockturtle::aig_network::node> nodes_rank0{1, 2, 3};
    std::vector<mockturtle::aig_network::node> nodes_rank1{4, 5};

    /*aig_r.modify_rank(0, nodes_rank0);
    aig_r.modify_rank(0, nodes_rank0);*/

    const auto layout = orthogonal_planar<gate_layout>(aig_r);

    debug::write_dot_layout(layout);
    debug::write_dot_network(fo_ntk);
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

    extended_rank_view aig_r(fo_ntk);

    std::vector<mockturtle::aig_network::node> nodes_rank0{1, 2, 3};
    std::vector<mockturtle::aig_network::node> nodes_rank1{4, 5};

    /*aig_r.modify_rank(0, nodes_rank0);
    aig_r.modify_rank(0, nodes_rank0);*/

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

    extended_rank_view aig_r(fo_ntk);

    std::vector<mockturtle::aig_network::node> nodes_rank0{1, 2, 3};
    std::vector<mockturtle::aig_network::node> nodes_rank1{4, 5};

    /*aig_r.modify_rank(0, nodes_rank0);
    aig_r.modify_rank(0, nodes_rank0);*/

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

    auto planarized_maj = fiction::node_duplication_planarization<technology_network>(maj_tec_b);

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

    extended_rank_view aig_r(fo_ntk);

    std::vector<mockturtle::aig_network::node> nodes_rank0{7, 6, 9, 11};
    std::vector<mockturtle::aig_network::node> nodes_rank1{8, 10, 12};

    aig_r.modify_rank(1, nodes_rank0);
    aig_r.modify_rank(2, nodes_rank1);

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

    extended_rank_view aig_r(fo_ntk);

    std::vector<mockturtle::aig_network::node> nodes_rank0{7, 6, 9, 11};
    std::vector<mockturtle::aig_network::node> nodes_rank1{8, 10, 12};

    aig_r.modify_rank(1, nodes_rank0);
    aig_r.modify_rank(2, nodes_rank1);

    const auto layout = orthogonal_planar<gate_layout>(aig_r);

    debug::write_dot_layout(layout);
    debug::write_dot_network(fo_ntk);
}