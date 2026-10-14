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
 * @brief Tests for `fiction/synthesis/planar_scheduling.hpp`.
 * @author Benjamin Hien (hibenj)
 */

#include <catch2/catch_test_macros.hpp>

#include "utils/blueprints/network_blueprints.hpp"

#include <fiction/networks/technology_network.hpp>
#include <fiction/networks/views/mutable_rank_view.hpp>
#include <fiction/synthesis/fanout_substitution.hpp>
#include <fiction/synthesis/network_balancing.hpp>
#include <fiction/synthesis/network_conversion.hpp>
#include <fiction/synthesis/planar_scheduling.hpp>
#include <fiction/synthesis/planarization.hpp>
#include <fiction/utils/graph/mincross.hpp>
#include <fiction/verification/virtual_miter.hpp>

#include <mockturtle/algorithms/equivalence_checking.hpp>
#include <mockturtle/networks/aig.hpp>

#include <cstdint>
#include <sstream>
#include <string>

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
 * Checks the contract of the scheduling on a planar result of the planarization pipeline.
 */
template <typename Spec, typename Ntk>
planar_scheduling_stats check_scheduling(const Spec& spec, const Ntk& planar)
{
    REQUIRE(count_crossings(planar) == 0);

    planar_scheduling_stats st{};
    const auto              scheduled = planar_scheduling(planar, {}, &st);

    CHECK(is_balanced(scheduled, {.unify_outputs = true, .buffer_constant_outputs = false}));
    CHECK(is_fanout_substituted(scheduled));
    CHECK(count_crossings(scheduled) == 0);
    CHECK(scheduled.num_gates() == st.nodes_after);
    CHECK(st.nodes_after <= st.nodes_before);
    CHECK(scheduled.num_pis() == planar.num_pis());
    CHECK(scheduled.num_pos() == planar.num_pos());
    CHECK(scheduled.depth() == planar.depth());
    check_equivalent(spec, scheduled);

    return st;
}

}  // namespace

TEST_CASE("A gate above buffers moves down and saves buffers", "[planar-scheduling]")
{
    // and(a, b) read through two chain buffers, output two levels up: the planarization pads it that way when c
    // joins late
    technology_network ntk{};
    const auto         a = ntk.create_pi();
    const auto         b = ntk.create_pi();
    const auto         c = ntk.create_pi();
    const auto         x = ntk.create_and(a, b);
    const auto         y = ntk.create_and(ntk.create_buf(ntk.create_buf(c)), ntk.create_buf(x));
    ntk.create_po(y);

    const auto              balanced = network_balancing<technology_network>(ntk, {.unify_outputs = true});
    const mutable_rank_view unordered{balanced};
    const auto              ranked = utils::graph::mincross(unordered, {});
    REQUIRE(count_crossings(ranked) == 0);

    // the AND of a and b sits on level 1 with a buffer above it; nothing to gain there, but the top AND reads its
    // fanins through chain buffers on level 2 (c's chain and x's buffer) and can move down to level 2
    planar_scheduling_stats st{};
    const auto              scheduled = planar_scheduling(ranked, {}, &st);
    CHECK(st.nodes_after < st.nodes_before);
    CHECK(st.gates_moved_down >= 1);
    CHECK(is_balanced(scheduled, {.unify_outputs = true}));
    CHECK(count_crossings(scheduled) == 0);
    check_equivalent(ntk, scheduled);

    std::ostringstream out{};
    st.report(out);
    CHECK(out.str().find("gates down") != std::string::npos);
}

TEST_CASE("Planarized blueprints keep their contract and shrink", "[planar-scheduling]")
{
    const auto check = [](const auto& aig)
    {
        const auto               tec = convert_network<technology_network>(aig);
        network_balancing_params bps{};
        bps.unify_outputs                = true;
        bps.buffer_constant_outputs      = false;
        const auto              balanced = network_balancing<technology_network>(tec, bps);
        const mutable_rank_view ranked{balanced};
        const auto              ordered = utils::graph::mincross(ranked, {});
        const auto              planar  = planarization(ordered);
        return check_scheduling(aig, planar);
    };

    for (const auto& st : {check(blueprints::maj4_network<mockturtle::aig_network>()),
                           check(blueprints::full_adder_network<mockturtle::aig_network>()),
                           check(blueprints::mux21_network<mockturtle::aig_network>()),
                           check(blueprints::parity_network<mockturtle::aig_network>()),
                           check(blueprints::nand_xnor_network<mockturtle::aig_network>()),
                           check(blueprints::se_coloring_corner_case_network<mockturtle::aig_network>())})
    {
        CHECK(st.passes >= 1);
    }
}
