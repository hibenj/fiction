//
// Created by benjamin on 21.01.25.
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

#include <fiction/algorithms/network_transformation/inverter_substitution.hpp>
#include <fiction/algorithms/physical_design/apply_gate_library.hpp>
#include <fiction/algorithms/physical_design/orthogonal_planar_v2.hpp>
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

template<typename Ntk, typename Lyt>
void check_properties(Ntk& ntk, Lyt& layout)
{
    fiction::gate_level_drv_params ps_post{};
    fiction::gate_level_drv_stats  st_post{};

    fiction::gate_level_drvs(layout, ps_post, &st_post);

    const auto miter = mockturtle::miter<mockturtle::klut_network>(ntk, layout);
    bool       eq    = false;
    if (miter)
    {
        mockturtle::equivalence_checking_stats st;

        const auto ce = mockturtle::equivalence_checking(*miter, {}, &st);
        eq            = ce.value();
    }

    CHECK (eq == true);

    if (eq == true)
    {
        std::cout << "is true\n";
    }
}

TEST_CASE("Multi Outputs", "[orthogonal-planar-v2]")
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

    const auto layout = orthogonal_planar_v2<gate_layout>(aig_r);

    check_properties(aig_r, layout);
}

TEST_CASE("Case 2-FOs", "[orthogonal-planar-v2]")
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

    const auto layout = orthogonal_planar_v2<gate_layout>(aig_r);

    check_properties(aig_r, layout);
}

TEST_CASE("Case 3-FOs", "[orthogonal-planar-v2]")
{
    using gate_layout = gate_level_layout<clocked_layout<tile_based_layout<cartesian_layout<offset::ucoord_t>>>>;

    virtual_pi_network<technology_network> ntk{};

    const auto x1 = ntk.create_pi();
    const auto x2 = ntk.create_pi();
    const auto x3 = ntk.create_pi();
    const auto x4 = ntk.create_pi();
    const auto x5 = ntk.create_pi();

    const auto b1 = ntk.create_buf(x1);
    const auto b2 = ntk.create_buf(x2);
    const auto b3 = ntk.create_buf(x3);
    const auto b4 = ntk.create_buf(x4);
    const auto b5 = ntk.create_buf(x5);

    const auto f1 = ntk.create_and(b1, b2);
    const auto f2 = ntk.create_not(b2);
    const auto f3 = ntk.create_not(b3);
    const auto f4 = ntk.create_not(b3);
    const auto f5 = ntk.create_not(b4);
    const auto f6 = ntk.create_and(b4, b5);

    const auto g1 = ntk.create_buf(f1);
    const auto g2 = ntk.create_and(f2, f3);
    const auto g3 = ntk.create_and(f4, f5);
    const auto g4 = ntk.create_buf(f6);

    ntk.create_po(g1);
    ntk.create_po(g2);
    ntk.create_po(g3);
    ntk.create_po(g4);

   mutable_rank_view aig_r(ntk);

    const auto layout = orthogonal_planar_v2<gate_layout>(aig_r);

    check_properties(aig_r, layout);
}

TEST_CASE("ANDs", "[orthogonal-planar-v2]")
{
    using gate_layout = gate_level_layout<clocked_layout<tile_based_layout<cartesian_layout<offset::ucoord_t>>>>;

    virtual_pi_network<technology_network> ntk{};

    const auto x1 = ntk.create_pi();
    const auto x2 = ntk.create_pi();
    const auto x3 = ntk.create_pi();
    const auto x4 = ntk.create_pi();
    const auto x5 = ntk.create_pi();

    const auto a1 = ntk.create_and(x1, x2);
    const auto a2 = ntk.create_and(x3, x4);
    const auto a3 = ntk.create_and(a1, a2);

    const auto b1 = ntk.create_not(x5);
    const auto b2 = ntk.create_not(b1);

    ntk.create_po(a3);
    ntk.create_po(b2);

    mutable_rank_view aig_r(ntk);

    const auto layout = orthogonal_planar_v2<gate_layout>(aig_r);

    check_properties(aig_r, layout);
}

TEST_CASE("Different Tests", "[orthogonal-planar-v2]")
{
    using gate_layout = gate_level_layout<clocked_layout<tile_based_layout<cartesian_layout<offset::ucoord_t>>>>;

    fiction::technology_network ntk;

    // Layer 0
    const auto pi0 = ntk.create_pi();
    const auto pi1 = ntk.create_pi();
    const auto pi2 = ntk.create_pi();
    const auto pi3 = ntk.create_pi();
    const auto pi4 = ntk.create_pi();
    const auto pi5 = ntk.create_pi();
    const auto pi6 = ntk.create_pi();
    const auto pi7 = ntk.create_pi();
    const auto pi8 = ntk.create_pi();
    const auto pi9 = ntk.create_pi();
    const auto pi10 = ntk.create_pi();
    const auto pi11 = ntk.create_pi();

    // Layer 1
    const auto b0 = ntk.create_buf(pi0);
    const auto b1 = ntk.create_buf(pi1);
    const auto b2 = ntk.create_buf(pi2);
    const auto b3 = ntk.create_buf(pi3);
    const auto b4 = ntk.create_buf(pi4);
    const auto b5 = ntk.create_buf(pi5);
    const auto b6 = ntk.create_buf(pi6);
    const auto b7 = ntk.create_buf(pi7);
    const auto b8 = ntk.create_buf(pi8);
    const auto b9 = ntk.create_buf(pi9);
    const auto b10 = ntk.create_buf(pi10);
    const auto b11 = ntk.create_buf(pi11);

    // Layer 2
    const auto c0 = ntk.create_buf(b0);
    const auto c1 = ntk.create_buf(b1);
    const auto c2 = ntk.create_buf(b2);
    const auto c3 = ntk.create_buf(b3);
    const auto c4 = ntk.create_buf(b3);
    const auto c5 = ntk.create_buf(b4);
    const auto c6 = ntk.create_buf(b5);
    const auto c7 = ntk.create_buf(b6);
    const auto c8 = ntk.create_buf(b7);
    const auto c9 = ntk.create_buf(b8);
    const auto c10 = ntk.create_buf(b8);
    const auto c11 = ntk.create_buf(b9);
    const auto c12 = ntk.create_buf(b10);
    const auto c13 = ntk.create_buf(b11);

    // Layer 3
    const auto d0 = ntk.create_buf(c0);
    const auto d1 = ntk.create_buf(c1);
    const auto d2 = ntk.create_buf(c2);
    const auto d3 = ntk.create_buf(c3);
    const auto d4 = ntk.create_buf(c3);
    const auto d5 = ntk.create_buf(c4);
    const auto d6 = ntk.create_and(c5, c6);
    const auto d7 = ntk.create_buf(c6);
    const auto d8 = ntk.create_buf(c7);
    const auto d9 = ntk.create_buf(c8);
    const auto d10 = ntk.create_buf(c8);
    const auto d11 = ntk.create_and(c9, c10);
    const auto d12 = ntk.create_buf(c11);
    const auto d13 = ntk.create_buf(c12);
    const auto d14 = ntk.create_buf(c13);

    ntk.create_po(d0);
    ntk.create_po(d1);
    ntk.create_po(d2);
    ntk.create_po(d3);
    ntk.create_po(d4);
    ntk.create_po(d5);
    ntk.create_po(d6);
    ntk.create_po(d7);
    ntk.create_po(d8);
    ntk.create_po(d9);
    ntk.create_po(d10);
    ntk.create_po(d11);
    ntk.create_po(d12);
    ntk.create_po(d13);
    ntk.create_po(d14);

    network_balancing_params ps;
    ps.unify_outputs = true;

    const auto fo_ntk = network_balancing<technology_network>(fanout_substitution<technology_network>(ntk), ps);

    const detail::operation_mode mode = detail::operation_mode::AND_OR_ONLY;

    auto aig_r = mutable_rank_view(fo_ntk);

    const auto layout = orthogonal_planar_v2<gate_layout>(aig_r);

    check_properties(aig_r, layout);

    debug::write_dot_network(aig_r);
    debug::write_dot_layout(layout);
}

TEST_CASE("Test blueprints", "[orthogonal-planar]")
{
    using gate_layout = gate_level_layout<clocked_layout<tile_based_layout<cartesian_layout<offset::ucoord_t>>>>;

    auto maj = blueprints::multi_output_network<mockturtle::names_view<mockturtle::aig_network>>();

    auto maj_t = convert_network<technology_network>(maj);

    network_balancing_params ps;
    ps.unify_outputs = true;

    const auto maj_tec_b = network_balancing<technology_network>(fanout_substitution<technology_network>(maj_t), ps);

    auto planarized_maj = fiction::node_duplication_planarization<technology_network>(maj_tec_b);

    const auto layout = orthogonal_planar_v2<gate_layout>(planarized_maj);

    debug::write_dot_layout(layout);

    debug::write_dot_network(planarized_maj);

    check_properties(planarized_maj, layout);
}
