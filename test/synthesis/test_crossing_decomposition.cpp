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
 * @brief Tests for `fiction/synthesis/crossing_decomposition.hpp`.
 * @author Benjamin Hien (hibenj)
 */

#include <catch2/catch_test_macros.hpp>

#include <fiction/networks/technology_network.hpp>
#include <fiction/networks/views/mutable_rank_view.hpp>
#include <fiction/synthesis/crossing_decomposition.hpp>
#include <fiction/synthesis/network_balancing.hpp>
#include <fiction/utils/graph/mincross.hpp>
#include <fiction/verification/virtual_miter.hpp>

#include <kitty/constructors.hpp>
#include <kitty/dynamic_truth_table.hpp>
#include <kitty/operators.hpp>
#include <kitty/print.hpp>
#include <mockturtle/algorithms/equivalence_checking.hpp>

#include <cstdint>
#include <sstream>
#include <stdexcept>
#include <string>

using namespace fiction;
using namespace fiction::networks;
using namespace fiction::networks::views;
using namespace fiction::synthesis;
using namespace fiction::utils::graph;
using namespace fiction::verification;

namespace
{

/**
 * Figure 7 of the IEEE NANO 2024 paper: two 4-LUTs x = a b ~c d and y = ~a b ~c d over the same four inputs. The LUTs
 * share all four fanins, so their fanin edges cross six times in any order.
 */
technology_network figure_7_network()
{
    technology_network tec{};

    const auto a = tec.create_pi();
    const auto b = tec.create_pi();
    const auto c = tec.create_pi();
    const auto d = tec.create_pi();

    kitty::dynamic_truth_table va{4u};
    kitty::dynamic_truth_table vb{4u};
    kitty::dynamic_truth_table vc{4u};
    kitty::dynamic_truth_table vd{4u};
    kitty::create_nth_var(va, 0u);
    kitty::create_nth_var(vb, 1u);
    kitty::create_nth_var(vc, 2u);
    kitty::create_nth_var(vd, 3u);

    const auto x_tt = va & vb & ~vc & vd;
    const auto y_tt = ~va & vb & ~vc & vd;

    // pin the variable order the decomposition relies on: a is the least significant variable
    REQUIRE(kitty::to_hex(x_tt) == "0800");
    REQUIRE(kitty::to_hex(y_tt) == "0400");

    const auto x = tec.create_node({a, b, c, d}, x_tt);
    const auto y = tec.create_node({a, b, c, d}, y_tt);

    tec.create_po(x);
    tec.create_po(y);

    return tec;
}

/**
 * Counts the crossings of a ranked network without reordering it.
 */
template <typename Ntk>
uint64_t count_crossings(const Ntk& ntk)
{
    mincross_params ps{};
    ps.optimize = false;
    mincross_stats st{};

    mincross(ntk, ps, &st);

    return st.num_crossings;
}

/**
 * Checks that the network computes the same functions as the original.
 */
template <typename Spec, typename Impl>
void check_equivalent(const Spec& spec, const Impl& impl)
{
    const auto miter = virtual_miter<technology_network>(spec, impl);
    REQUIRE(miter.has_value());

    if (miter.has_value())  // the optional-access check does not model REQUIRE
    {
        mockturtle::equivalence_checking_stats st{};
        const auto                             cec = mockturtle::equivalence_checking(*miter, {}, &st);

        REQUIRE(cec.has_value());
        CHECK(cec.value_or(false));
    }
}

}  // namespace

TEST_CASE("Figure 7 starts with six crossings", "[crossing-decomposition]")
{
    const auto tec = figure_7_network();

    // every edge spans one level already, so balancing changes nothing
    const auto balanced = network_balancing<technology_network>(tec);
    REQUIRE(is_balanced(balanced));
    REQUIRE(balanced.size() == tec.size());

    crossing_decomposition_stats st{};
    const auto                   result = crossing_decomposition(balanced, {}, &st);

    CHECK(st.crossings_before == 6);
    CHECK(st.crossings_after == 6);
    CHECK(st.decompositions == 0);
    CHECK(st.levels_inserted == 0);

    // `mincross` agrees on the returned network
    CHECK(count_crossings(result) == 6);

    CHECK(result.depth() == 1);
    CHECK(is_balanced(result));
    CHECK(result.check_validity());
    check_equivalent(tec, result);
}

TEST_CASE("The initial ordering follows the mincross parameters", "[crossing-decomposition]")
{
    technology_network tec{};

    const auto x1 = tec.create_pi();
    const auto x2 = tec.create_pi();
    const auto x3 = tec.create_pi();
    const auto n  = tec.create_not(x2);
    const auto f  = tec.create_and(x1, x3);
    tec.create_po(f);
    tec.create_po(n);

    crossing_decomposition_params ps{};
    crossing_decomposition_stats  st{};

    // free inputs: x2 moves aside and the single crossing disappears
    ps.mincross.fixed_pis = false;
    const auto free_pis   = crossing_decomposition(tec, ps, &st);
    CHECK(st.crossings_before == 0);
    CHECK(count_crossings(free_pis) == 0);

    // fixed inputs: the crossing between x1 -> f and x2 -> n stays
    ps.mincross.fixed_pis = true;
    const auto fixed_pis  = crossing_decomposition(tec, ps, &st);
    CHECK(st.crossings_before == 1);
    CHECK(count_crossings(fixed_pis) == 1);
}

TEST_CASE("Unbalanced networks are rejected", "[crossing-decomposition]")
{
    technology_network tec{};

    const auto x1 = tec.create_pi();
    const auto x2 = tec.create_pi();
    tec.create_po(tec.create_and(x1, tec.create_buf(x2)));

    REQUIRE(!is_balanced(tec));

    CHECK_THROWS_AS(crossing_decomposition(tec), std::invalid_argument);
}

TEST_CASE("Statistics report", "[crossing-decomposition]")
{
    crossing_decomposition_stats st{};
    st.crossings_before = 6;
    st.crossings_after  = 1;
    st.decompositions   = 1;
    st.levels_inserted  = 1;

    std::ostringstream out{};
    st.report(out);

    const auto text = out.str();

    CHECK(text.find("crossings before  = 6") != std::string::npos);
    CHECK(text.find("crossings after   = 1") != std::string::npos);
    CHECK(text.find("decompositions    = 1") != std::string::npos);
    CHECK(text.find("levels inserted   = 1") != std::string::npos);
}
