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
#include <kitty/decomposition.hpp>
#include <kitty/dynamic_truth_table.hpp>
#include <kitty/operators.hpp>
#include <kitty/print.hpp>
#include <mockturtle/algorithms/equivalence_checking.hpp>
#include <mockturtle/traits.hpp>

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

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

using node = mockturtle::node<technology_network>;

/**
 * Creates the variable `index` of a truth table over `num_vars` variables.
 */
kitty::dynamic_truth_table var(uint32_t num_vars, uint8_t index)
{
    kitty::dynamic_truth_table tt{num_vars};
    kitty::create_nth_var(tt, index);
    return tt;
}

/**
 * Checks a found decomposition of f over `fanins` against kitty's Ashenhurst oracle: the outer function g over the
 * fanins that are not shared-node inputs plus the shared node must compose back to f.
 */
void check_oracle(const kitty::dynamic_truth_table& f, const std::vector<node>& fanins,
                  const std::vector<node>& g_fanins, const kitty::dynamic_truth_table& g,
                  const fiction::synthesis::detail::shared_decomposition<node>& dec)
{
    std::vector<uint32_t> ys{};
    std::vector<uint32_t> zs{};

    for (const auto& n : dec.h_fanins)
    {
        const auto it = std::find(fanins.cbegin(), fanins.cend(), n);
        REQUIRE(it != fanins.cend());
        ys.push_back(static_cast<uint32_t>(std::distance(fanins.cbegin(), it)));
    }
    for (const auto& n : g_fanins)
    {
        const auto it = std::find(fanins.cbegin(), fanins.cend(), n);
        REQUIRE(it != fanins.cend());
        zs.push_back(static_cast<uint32_t>(std::distance(fanins.cbegin(), it)));
    }

    CHECK(g.num_vars() == zs.size() + 1);
    CHECK(kitty::is_ashenhurst_decomposable(f, zs, ys, g, dec.h));
}

}  // namespace

TEST_CASE("Figure 7 LUT pair shares b ~c d", "[crossing-decomposition]")
{
    // x = a b ~c d and y = ~a b ~c d over the fanins a, b, c, d (node ids 1 to 4)
    const std::vector<node> fanins{1, 2, 3, 4};

    const auto f1 = var(4, 0) & var(4, 1) & ~var(4, 2) & var(4, 3);
    const auto f2 = ~var(4, 0) & var(4, 1) & ~var(4, 2) & var(4, 3);

    const auto dec = fiction::synthesis::detail::find_shared_decomposition(f1, fanins, f2, fanins);

    REQUIRE(dec.has_value());

    // the shared node is h = b ~c d; a stays with each LUT, which becomes a h and ~a h
    CHECK(dec->h_fanins == std::vector<node>{2, 3, 4});
    CHECK(dec->h == (var(3, 0) & ~var(3, 1) & var(3, 2)));
    CHECK(dec->g1_fanins == std::vector<node>{1});
    CHECK(dec->g1 == (var(2, 0) & var(2, 1)));
    CHECK(dec->g2_fanins == std::vector<node>{1});
    CHECK(dec->g2 == (~var(2, 0) & var(2, 1)));

    check_oracle(f1, fanins, dec->g1_fanins, dec->g1, *dec);
    check_oracle(f2, fanins, dec->g2_fanins, dec->g2, *dec);
}

TEST_CASE("Two shared inputs with a common XOR", "[crossing-decomposition]")
{
    // f1 = (a ^ b) & c over a, b, c; f2 = d | (a ^ b) over d, a, b: the shared inputs sit at different positions
    const std::vector<node> fanins1{1, 2, 3};
    const std::vector<node> fanins2{4, 1, 2};

    const auto f1 = (var(3, 0) ^ var(3, 1)) & var(3, 2);
    const auto f2 = var(3, 0) | (var(3, 1) ^ var(3, 2));

    const auto dec = fiction::synthesis::detail::find_shared_decomposition(f1, fanins1, f2, fanins2);

    REQUIRE(dec.has_value());

    CHECK(dec->h_fanins == std::vector<node>{1, 2});
    CHECK(dec->h == (var(2, 0) ^ var(2, 1)));
    CHECK(dec->g1_fanins == std::vector<node>{3});
    CHECK(dec->g1 == (var(2, 0) & var(2, 1)));
    CHECK(dec->g2_fanins == std::vector<node>{4});
    CHECK(dec->g2 == (var(2, 0) | var(2, 1)));

    check_oracle(f1, fanins1, dec->g1_fanins, dec->g1, *dec);
    check_oracle(f2, fanins2, dec->g2_fanins, dec->g2, *dec);
}

TEST_CASE("The largest shared node is preferred", "[crossing-decomposition]")
{
    // f1 = a ^ b ^ c ^ d and f2 = ~(a ^ b ^ c ^ d): the parity of all four inputs is the shared node, the LUTs
    // collapse to a buffer and an inverter of it
    const std::vector<node> fanins{1, 2, 3, 4};

    const auto f1 = var(4, 0) ^ var(4, 1) ^ var(4, 2) ^ var(4, 3);
    const auto f2 = ~f1;

    const auto dec = fiction::synthesis::detail::find_shared_decomposition(f1, fanins, f2, fanins);

    REQUIRE(dec.has_value());

    CHECK(dec->h_fanins == fanins);
    CHECK(dec->h == f1);
    CHECK(dec->g1_fanins.empty());
    CHECK(dec->g1 == var(1, 0));
    CHECK(dec->g2_fanins.empty());
    CHECK(dec->g2 == ~var(1, 0));

    check_oracle(f1, fanins, dec->g1_fanins, dec->g1, *dec);
    check_oracle(f2, fanins, dec->g2_fanins, dec->g2, *dec);
}

TEST_CASE("Shared inputs without a common sub-function", "[crossing-decomposition]")
{
    // a & b needs h to single out the minterm 11, a | b needs it to single out 00: no binary h does both
    const std::vector<node> fanins{1, 2};

    const auto f1 = var(2, 0) & var(2, 1);
    const auto f2 = var(2, 0) | var(2, 1);

    CHECK(!fiction::synthesis::detail::find_shared_decomposition(f1, fanins, f2, fanins).has_value());

    // with a third shared input c, the pair a & b & c and (a | b) & c has no common sub-function either: h would have
    // to be over {a, b} (impossible, see above), over {a, c}, {b, c} (the other variable is symmetric in neither) or
    // over all three (impossible for the same reason as {a, b})
    const std::vector<node> fanins3{1, 2, 3};

    const auto f3 = var(3, 0) & var(3, 1) & var(3, 2);
    const auto f4 = (var(3, 0) | var(3, 1)) & var(3, 2);

    CHECK(!fiction::synthesis::detail::find_shared_decomposition(f3, fanins3, f4, fanins3).has_value());
}

TEST_CASE("Fewer than two shared inputs", "[crossing-decomposition]")
{
    const std::vector<node> fanins1{1, 2};
    const std::vector<node> fanins2{2, 3};

    const auto f1 = var(2, 0) & var(2, 1);
    const auto f2 = var(2, 0) & var(2, 1);

    CHECK(!fiction::synthesis::detail::find_shared_decomposition(f1, fanins1, f2, fanins2).has_value());

    const std::vector<node> fanins3{3, 4};

    CHECK(!fiction::synthesis::detail::find_shared_decomposition(f1, fanins1, f2, fanins3).has_value());
}

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
