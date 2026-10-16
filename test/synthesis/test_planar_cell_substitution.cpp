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
 * @brief Tests for `fiction/synthesis/planar_cell_substitution.hpp`.
 * @author Benjamin Hien (hibenj)
 */

#include <catch2/catch_test_macros.hpp>

#include "utils/blueprints/network_blueprints.hpp"

#include <fiction/networks/technology_network.hpp>
#include <fiction/networks/views/mutable_rank_view.hpp>
#include <fiction/synthesis/network_balancing.hpp>
#include <fiction/synthesis/network_conversion.hpp>
#include <fiction/synthesis/planar_cell_database.hpp>
#include <fiction/synthesis/planar_cell_substitution.hpp>
#include <fiction/utils/graph/mincross.hpp>
#include <fiction/verification/virtual_miter.hpp>

#include <kitty/constructors.hpp>
#include <kitty/dynamic_truth_table.hpp>
#include <kitty/operations.hpp>
#include <mockturtle/algorithms/equivalence_checking.hpp>
#include <mockturtle/algorithms/lut_mapper.hpp>
#include <mockturtle/networks/aig.hpp>
#include <mockturtle/networks/klut.hpp>
#include <mockturtle/utils/node_map.hpp>
#include <mockturtle/views/topo_view.hpp>

#include <cstdint>
#include <sstream>
#include <vector>

using namespace fiction;
using namespace fiction::networks;
using namespace fiction::networks::views;
using namespace fiction::synthesis;
using namespace fiction::verification;

namespace
{

template <typename Spec, typename Impl>
void check_equivalent(const Spec& spec, const Impl& impl)
{
    const auto miter = virtual_miter<technology_network>(spec, impl);
    REQUIRE(miter.has_value());
    if (miter.has_value())
    {
        mockturtle::equivalence_checking_stats st{};
        const auto                             cec = mockturtle::equivalence_checking(*miter, {}, &st);
        REQUIRE(cec.has_value());
        CHECK(cec.value_or(false));
    }
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
 * Maps a network to 4-LUTs with truth tables of exactly the fanin count.
 */
template <typename Ntk>
technology_network lut_mapped(const Ntk& ntk)
{
    mockturtle::lut_map_params lps{};
    lps.cut_enumeration_ps.cut_size = 4u;
    const auto klut                 = mockturtle::lut_map(ntk, lps);

    mockturtle::klut_network                                                         dest{};
    mockturtle::node_map<mockturtle::klut_network::signal, mockturtle::klut_network> m{klut};
    m[klut.get_constant(false)] = dest.get_constant(false);
    m[klut.get_constant(true)]  = dest.get_constant(true);
    klut.foreach_pi([&](const auto& n) { m[n] = dest.create_pi(); });
    mockturtle::topo_view topo{klut};
    topo.foreach_gate(
        [&](const auto& n)
        {
            std::vector<mockturtle::klut_network::signal> cs{};
            klut.foreach_fanin(n, [&](const auto& f) { cs.push_back(m[klut.get_node(f)]); });
            auto tt = klut.node_function(n);
            if (tt.num_vars() > cs.size())
            {
                tt = kitty::shrink_to(tt, static_cast<uint32_t>(cs.size()));
            }
            m[n] = dest.create_node(cs, tt);
        });
    klut.foreach_po([&](const auto& po) { dest.create_po(m[klut.get_node(po)]); });

    return convert_network<technology_network>(dest);
}

/**
 * Checks the contract of the substitution on a ranked, balanced LUT network.
 */
template <typename Spec>
void check_substitution(const Spec& spec, const mutable_rank_view<technology_network>& ranked,
                        const planar_cell_database& db, planar_cell_substitution_stats& st,
                        const planar_cell_substitution_params& ps = {})
{
    const auto before = count_crossings(ranked);
    const auto result = planar_cell_substitution(ranked, db, ps, &st);

    CHECK(is_balanced(result));
    CHECK(count_crossings(result) == before);
    CHECK(result.depth() == st.levels_after);
    CHECK(result.depth() >= ranked.depth());
    CHECK(result.num_pis() == ranked.num_pis());
    CHECK(result.num_pos() == ranked.num_pos());
    check_equivalent(spec, result);

    // every result node is a gate of the gate set, an inverter or a buffer; with absorbed inversions also one of
    // the other two-input functions
    result.foreach_gate(
        [&result, &ps](const auto& n)
        {
            const bool plain = result.is_and(n) || result.is_or(n) || result.is_xor(n) || result.is_maj(n) ||
                               result.is_inv(n) || result.is_buf(n);
            const bool absorbed =
                ps.absorb_inversions && (result.is_nand(n) || result.is_nor(n) || result.is_xnor(n) ||
                                         result.is_lt(n) || result.is_gt(n) || result.is_le(n) || result.is_ge(n));
            CHECK((plain || absorbed));
        });
}

}  // namespace

TEST_CASE("Cells keep the crossings and the function of a small LUT network", "[planar-cell-substitution]")
{
    planar_cell_database_params ps{};
    ps.num_inputs = 4;
    ps.max_levels = 3;
    const auto db = generate_planar_cells(ps);

    // two LUTs whose fanin edges cross once, joined by a third LUT
    technology_network ntk{};
    const auto         a = ntk.create_pi();
    const auto         b = ntk.create_pi();
    const auto         c = ntk.create_pi();
    const auto         d = ntk.create_pi();

    kitty::dynamic_truth_table and2{2};
    kitty::dynamic_truth_table xnor2{2};
    kitty::dynamic_truth_table maj3{3};
    kitty::create_from_binary_string(and2, "1000");
    kitty::create_from_binary_string(xnor2, "1001");
    kitty::create_majority(maj3);

    const auto l1 = ntk.create_node({a, c}, and2);   // reads a and c
    const auto l2 = ntk.create_node({b, d}, xnor2);  // reads b and d: crosses the edge c -> l1
    const auto l3 = ntk.create_node({l1, l2, a}, maj3);
    ntk.create_po(l3);
    ntk.create_po(l2);

    const auto              balanced = network_balancing<technology_network>(ntk, {.share_buffers = true});
    const mutable_rank_view ranked{balanced};
    CHECK(count_crossings(ranked) > 0);

    planar_cell_substitution_stats st{};
    check_substitution(ntk, ranked, db, st);
    CHECK(st.luts == 3);
    CHECK(st.luts_with_cell == 3);
    CHECK(st.luts_kept == 0);
    CHECK(st.gates == 3);
    CHECK(st.levels_before == 2);
    CHECK(st.levels_after >= 2);

    std::ostringstream out{};
    st.report(out);
    CHECK(out.str().find("LUTs = 3") != std::string::npos);
}

TEST_CASE("Cells replace the LUTs of mapped blueprints", "[planar-cell-substitution]")
{
    planar_cell_database_params ps{};
    ps.num_inputs = 4;
    ps.max_levels = 4;
    const auto db = generate_planar_cells(ps);

    const auto check = [&db](const auto& aig)
    {
        const auto              mapped   = lut_mapped(aig);
        const auto              balanced = network_balancing<technology_network>(mapped, {.share_buffers = true});
        const mutable_rank_view ranked{balanced};

        planar_cell_substitution_stats st{};
        check_substitution(aig, ranked, db, st);
        CHECK(st.luts_with_cell == st.luts);
        CHECK(st.luts_kept == 0);
    };

    check(blueprints::maj4_network<mockturtle::aig_network>());
    check(blueprints::full_adder_network<mockturtle::aig_network>());
    check(blueprints::mux21_network<mockturtle::aig_network>());
    check(blueprints::parity_network<mockturtle::aig_network>());
    check(blueprints::nand_xnor_network<mockturtle::aig_network>());
}

TEST_CASE("A LUT without a cell is kept", "[planar-cell-substitution]")
{
    // one level: only majority cells, so an AND LUT of two inputs has no cell and stays a LUT
    planar_cell_database_params ps{};
    ps.num_inputs = 3;
    ps.max_levels = 1;
    ps.gate_set   = {planar_gate::MAJ};
    const auto db = generate_planar_cells(ps);

    technology_network ntk{};
    const auto         a = ntk.create_pi();
    const auto         b = ntk.create_pi();
    const auto         c = ntk.create_pi();
    const auto         x = ntk.create_and(a, b);
    const auto         y = ntk.create_maj(x, b, c);
    ntk.create_po(y);

    const auto              balanced = network_balancing<technology_network>(ntk, {.share_buffers = true});
    const mutable_rank_view ranked{balanced};

    planar_cell_substitution_stats st{};
    check_substitution(ntk, ranked, db, st);
    CHECK(st.luts == 2);
    CHECK(st.luts_kept == 1);
    CHECK(st.luts_with_cell == 1);
}

TEST_CASE("Absorbed inversions take no level", "[planar-cell-substitution]")
{
    planar_cell_database_params ps{};
    ps.num_inputs = 4;
    ps.max_levels = 3;
    const auto db = generate_planar_cells(ps);

    planar_cell_substitution_params absorb{};
    absorb.absorb_inversions = true;

    // the crossing network of the first test, now with an inverter of the input in front of one LUT
    technology_network ntk{};
    const auto         a = ntk.create_pi();
    const auto         b = ntk.create_pi();
    const auto         c = ntk.create_pi();
    const auto         d = ntk.create_pi();

    kitty::dynamic_truth_table and2{2};
    kitty::dynamic_truth_table xnor2{2};
    kitty::dynamic_truth_table maj3{3};
    kitty::create_from_binary_string(and2, "1000");
    kitty::create_from_binary_string(xnor2, "1001");
    kitty::create_majority(maj3);

    const auto nd = ntk.create_not(d);
    const auto l1 = ntk.create_node({a, c}, and2);
    const auto l2 = ntk.create_node({b, nd}, xnor2);
    const auto l3 = ntk.create_node({l1, l2, a}, maj3);
    ntk.create_po(l3);
    ntk.create_po(l2);

    const auto              balanced = network_balancing<technology_network>(ntk, {.share_buffers = true});
    const mutable_rank_view ranked{balanced};

    planar_cell_substitution_stats with_inverters{};
    check_substitution(ntk, ranked, db, with_inverters);
    CHECK(with_inverters.inverters >= 1);  // the xnor needs an inversion
    CHECK(with_inverters.inversions_absorbed == 0);
    CHECK(with_inverters.input_inverters == 1);

    planar_cell_substitution_stats absorbed{};
    check_substitution(ntk, ranked, db, absorbed, absorb);
    CHECK(absorbed.luts == 3);
    CHECK(absorbed.luts_with_cell == 3);
    CHECK(absorbed.gates == with_inverters.gates);
    CHECK(absorbed.inverters == 0);
    CHECK(absorbed.inversions_absorbed == with_inverters.inverters);
    CHECK(absorbed.input_inverters == 1);
    CHECK(absorbed.levels_after < with_inverters.levels_after);
    CHECK(absorbed.levels_before == 3);  // the inverter of the input is a level of its own
    CHECK(absorbed.levels_after == 3);   // the cells add none

    std::ostringstream out{};
    absorbed.report(out);
    CHECK(out.str().find("1 of the input") != std::string::npos);

    // mapped blueprints under absorption: equivalent, no inverters inside cells
    const auto check = [&db, &absorb](const auto& aig)
    {
        const auto              mapped = lut_mapped(aig);
        const auto              bal    = network_balancing<technology_network>(mapped, {.share_buffers = true});
        const mutable_rank_view r{bal};

        planar_cell_substitution_stats st{};
        check_substitution(aig, r, db, st, absorb);
        CHECK(st.luts_with_cell == st.luts);
        CHECK(st.inverters == 0);
    };

    check(blueprints::maj4_network<mockturtle::aig_network>());
    check(blueprints::full_adder_network<mockturtle::aig_network>());
    check(blueprints::mux21_network<mockturtle::aig_network>());
    check(blueprints::parity_network<mockturtle::aig_network>());
    check(blueprints::nand_xnor_network<mockturtle::aig_network>());
}
