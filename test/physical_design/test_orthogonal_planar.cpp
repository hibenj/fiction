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
 * @brief Tests for `fiction/physical_design/orthogonal_planar.hpp`.
 * @author Benjamin Hien (hibenj)
 */

#include <catch2/catch_test_macros.hpp>

#include "utils/blueprints/network_blueprints.hpp"
#include "utils/equivalence_checking_utils.hpp"
#include "utils/network_ranking_utils.hpp"

#include <fiction/layouts/clocking_scheme.hpp>
#include <fiction/layouts/gate_layout.hpp>
#include <fiction/networks/extract_layout_network.hpp>
#include <fiction/networks/network_utils.hpp>
#include <fiction/networks/technology_network.hpp>
#include <fiction/physical_design/apply_gate_library.hpp>
#include <fiction/physical_design/orthogonal_planar.hpp>
#include <fiction/synthesis/planarization.hpp>
#include <fiction/technology/qca/qca_one_library.hpp>

#include <mockturtle/networks/aig.hpp>
#include <mockturtle/networks/mig.hpp>

#include <stdexcept>

using namespace fiction;
using namespace fiction::layouts;
using namespace fiction::networks;
using namespace fiction::networks::views;
using namespace fiction::physical_design;
using namespace fiction::synthesis;

namespace
{

void check_stats(const orthogonal_planar_stats& st)
{
    CHECK(st.x_size > 0);
    CHECK(st.y_size > 0);
    CHECK(st.num_gates > 0);
    CHECK(st.num_wires > 0);
    CHECK(st.num_crossings == 0);
    CHECK(st.rounds >= 1);
}

/**
 * Planarizes the network, places and routes it, and checks that the layout is crossing-free and computes the planar
 * network's functions.
 */
template <typename Ntk>
void check_orthogonal_planar(const Ntk& ntk, const bool xor_gates,
                             const clocking::num_clks phases = clocking::num_clks::FOUR)
{
    planarization_params p_ps{};
    p_ps.duplication.crossing_gates.xor_gates = xor_gates;

    const auto planar = planarization(rank(ntk), p_ps);

    orthogonal_planar_params ps{};
    ps.number_of_clock_phases = phases;
    orthogonal_planar_stats st{};

    const auto layout = orthogonal_planar<cartesian_gate_layout>(planar, ps, &st);

    check_stats(st);
    CHECK(layout.num_crossings() == 0);
    CHECK(layout.num_pis() == planar.num_pis());
    CHECK(layout.num_pos() == planar.num_pos());
    check_eq(planar, extract_layout_network(layout));
}

template <typename Ntk>
void check_both_crossing_gates(const Ntk& ntk)
{
    check_orthogonal_planar(ntk, false);
    check_orthogonal_planar(ntk, true);
}

/**
 * Places the planarized network twice and checks that both layouts agree tile by tile.
 */
template <typename Ntk>
void check_deterministic(const Ntk& ntk)
{
    const auto planar = planarization(rank(ntk));

    const auto first  = orthogonal_planar<cartesian_gate_layout>(planar);
    const auto second = orthogonal_planar<cartesian_gate_layout>(planar);

    REQUIRE(first.width() == second.width());
    REQUIRE(first.height() == second.height());

    first.foreach_coordinate(
        [&first, &second](const auto& c)
        {
            CHECK(first.is_empty_tile(c) == second.is_empty_tile(c));
            CHECK(first.is_gate_tile(c) == second.is_gate_tile(c));
            CHECK(first.is_wire_tile(c) == second.is_wire_tile(c));
            CHECK(first.is_pi_tile(c) == second.is_pi_tile(c));
            CHECK(first.is_po_tile(c) == second.is_po_tile(c));
            CHECK(first.incoming_data_flow(c) == second.incoming_data_flow(c));
        });
}

}  // namespace

TEST_CASE("Layout equivalence", "[orthogonal-planar]")
{
    check_both_crossing_gates(blueprints::unbalanced_and_inv_network<mockturtle::aig_network>());
    check_both_crossing_gates(blueprints::maj1_network<mockturtle::aig_network>());
    check_both_crossing_gates(blueprints::maj4_network<mockturtle::aig_network>());
    check_both_crossing_gates(blueprints::half_adder_network<mockturtle::mig_network>());
    check_both_crossing_gates(blueprints::full_adder_network<mockturtle::mig_network>());
    check_both_crossing_gates(blueprints::mux21_network<technology_network>());
    check_both_crossing_gates(blueprints::parity_network<technology_network>());
    check_both_crossing_gates(blueprints::clpl<technology_network>());
    check_both_crossing_gates(blueprints::multi_output_network<technology_network>());
    check_both_crossing_gates(blueprints::se_coloring_corner_case_network<technology_network>());
    check_both_crossing_gates(blueprints::fanout_substitution_corner_case_network<technology_network>());
    check_both_crossing_gates(blueprints::nary_operation_network<technology_network>());
}

TEST_CASE("Determinism", "[orthogonal-planar]")
{
    check_deterministic(blueprints::mux21_network<technology_network>());
    check_deterministic(blueprints::clpl<technology_network>());
    check_deterministic(blueprints::se_coloring_corner_case_network<technology_network>());
    check_deterministic(blueprints::fanout_substitution_corner_case_network<technology_network>());
    check_deterministic(blueprints::parity_network<technology_network>());
    check_deterministic(blueprints::full_adder_network<mockturtle::mig_network>());
}

TEST_CASE("A demand on the gap below a fanout node with an east gate and a south buffer", "[orthogonal-planar]")
{
    // built level by level in rank order, already balanced and planar: f drives the gate g (its east consumer) and
    // the buffer s (its south consumer); the consumers of g's and s's fanout nodes meet on the last level, where the
    // south consumer of g8 sits left of the east consumer of s1, so their gap demands a wider gap between g and s,
    // which only the fanout root of f can pay
    technology_network tec{};

    const auto a = tec.create_pi();
    const auto b = tec.create_pi();

    const auto n = tec.create_buf(a);
    const auto f = tec.create_buf(b);

    const auto g = tec.create_and(n, f);
    const auto s = tec.create_buf(f);

    const auto g8 = tec.create_buf(g);
    const auto s1 = tec.create_buf(s);
    const auto s2 = tec.create_not(s);

    tec.create_po(tec.create_buf(g8));
    tec.create_po(tec.create_not(g8));
    tec.create_po(tec.create_buf(s1));
    tec.create_po(tec.create_not(s1));
    tec.create_po(tec.create_buf(s2));

    const auto ranked = rank(tec);

    orthogonal_planar_params ps{};
    ps.max_rounds = 8;
    orthogonal_planar_stats st{};

    const auto layout = orthogonal_planar<cartesian_gate_layout>(ranked, ps, &st);

    CHECK(st.rounds == 2);
    CHECK(layout.num_crossings() == 0);
    check_eq(ranked, extract_layout_network(layout));
}

TEST_CASE("Three clock phases", "[orthogonal-planar]")
{
    check_orthogonal_planar(blueprints::full_adder_network<mockturtle::mig_network>(), true, clocking::num_clks::THREE);
    check_orthogonal_planar(blueprints::parity_network<technology_network>(), false, clocking::num_clks::THREE);
}

TEST_CASE("Gate library applies to the layout", "[orthogonal-planar]")
{
    const auto planar = planarization(rank(blueprints::full_adder_network<mockturtle::mig_network>()));
    const auto layout = orthogonal_planar<cartesian_gate_layout>(planar);

    CHECK_NOTHROW(apply_gate_library<qca::qca_one_library>(layout));
}

TEST_CASE("Rejected inputs", "[orthogonal-planar]")
{
    SECTION("a network with a three-input gate")
    {
        CHECK_THROWS_AS(orthogonal_planar<cartesian_gate_layout>(rank(blueprints::maj1_network<technology_network>())),
                        high_degree_fanin_exception);
    }
    SECTION("a ranked network whose rank order has crossings")
    {
        CHECK_THROWS_AS(
            orthogonal_planar<cartesian_gate_layout>(rank(blueprints::and_or_network<technology_network>())),
            std::invalid_argument);
    }
    SECTION("a network without inputs")
    {
        technology_network tec{};
        tec.create_po(tec.get_constant(true));

        CHECK_THROWS_AS(orthogonal_planar<cartesian_gate_layout>(rank(tec)), std::invalid_argument);
    }
    SECTION("an exhausted round budget")
    {
        const auto planar =
            planarization(rank(blueprints::fanout_substitution_corner_case_network<technology_network>()));

        orthogonal_planar_params ps{};
        ps.max_rounds = 1;

        CHECK_THROWS_AS(orthogonal_planar<cartesian_gate_layout>(planar, ps), std::runtime_error);

        ps.max_rounds = 2;
        orthogonal_planar_stats st{};
        CHECK_NOTHROW(orthogonal_planar<cartesian_gate_layout>(planar, ps, &st));
        CHECK(st.rounds == 2);
    }
}
