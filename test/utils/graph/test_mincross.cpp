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
 * @brief Tests for `fiction/utils/graph/mincross.hpp`.
 * @author Benjamin Hien (hibenj)
 * @author Marcel Walter (marcelwa)
 */

#include <catch2/catch_test_macros.hpp>

#include "utils/blueprints/network_blueprints.hpp"

#include <fiction/networks/technology_network.hpp>
#include <fiction/networks/views/bfs_topo_view.hpp>
#include <fiction/networks/views/mutable_rank_view.hpp>
#include <fiction/synthesis/network_balancing.hpp>
#include <fiction/utils/graph/mincross.hpp>

#include <mockturtle/networks/aig.hpp>
#include <mockturtle/traits.hpp>
#include <mockturtle/views/names_view.hpp>

#include <algorithm>
#include <vector>

using namespace fiction;
using namespace fiction::networks;
using namespace fiction::networks::views;
using namespace fiction::synthesis;
using namespace fiction::utils::graph;

TEST_CASE("Dummy Check", "[mincross]")
{
    mockturtle::aig_network aig;

    const auto x1 = aig.create_pi();
    const auto x2 = aig.create_pi();
    const auto f  = aig.create_xor(x1, x2);
    aig.create_po(f);

    const auto aig_r = mutable_rank_view(aig);

    mincross_stats        st{};
    const mincross_params p{};
    const auto            ntk = mincross(aig_r, p, &st);
    CHECK(st.num_crossings == 1);
}

TEST_CASE("Fixed PIs Check", "[mincross]")
{
    technology_network tec;

    const auto x1 = tec.create_pi();
    const auto x2 = tec.create_pi();
    const auto x3 = tec.create_pi();
    const auto b  = tec.create_not(x2);
    const auto f  = tec.create_and(x1, x3);
    tec.create_po(f);
    tec.create_po(b);

    const auto aig_r = mutable_rank_view(tec);

    mincross_stats  st{};
    mincross_params p{};
    p.fixed_pis = false;
    auto ntk    = mincross(aig_r, p, &st);
    CHECK(st.num_crossings == 0);

    p.fixed_pis = true;
    ntk         = mincross(aig_r, p, &st);
    CHECK(st.num_crossings == 1);
}

TEST_CASE("Planar Network", "[mincross]")
{
    mockturtle::aig_network aig;

    const auto x1 = aig.create_pi();
    const auto x2 = aig.create_pi();
    const auto x3 = aig.create_pi();
    const auto f1 = aig.create_and(x1, x2);
    const auto f2 = aig.create_and(x2, x3);
    aig.create_po(f1);
    aig.create_po(f2);

    auto                                                   aig_r = mutable_rank_view(aig);
    std::vector<mockturtle::node<mockturtle::aig_network>> rank1;
    rank1.push_back(aig_r.get_node(f2));
    rank1.push_back(aig_r.get_node(f1));
    aig_r.set_ranks(1, rank1);

    mincross_stats  st{};
    mincross_params p{};
    p.optimize = false;

    auto ntk = mincross(aig_r, p, &st);  // counts crossings
    CHECK(st.num_crossings == 3);

    p.optimize = true;
    ntk        = mincross(aig_r, p, &st);
    CHECK(st.num_crossings == 0);
}

TEST_CASE("Majority", "[mincross]")
{
    technology_network tec{};

    const auto x1 = tec.create_pi();
    const auto x2 = tec.create_pi();
    const auto x3 = tec.create_pi();
    const auto f1 = tec.create_maj(x1, x2, x3);
    const auto f2 = tec.create_not(x1);
    tec.create_po(f1);
    tec.create_po(f2);

    auto                                              tec_r = mutable_rank_view(tec);
    std::vector<mockturtle::node<technology_network>> rank1;
    rank1.push_back(tec_r.get_node(f1));
    rank1.push_back(tec_r.get_node(f2));
    tec_r.set_ranks(1, rank1);

    mincross_stats  st{};
    mincross_params p{};
    p.optimize = false;

    auto ntk = mincross(tec_r, p, &st);  // counts crossings
    CHECK(st.num_crossings == 2);

    p.optimize = true;
    ntk        = mincross(tec_r, p, &st);
    CHECK(st.num_crossings == 0);
}

TEST_CASE("Adder", "[mincross]")
{
    auto tec = blueprints::full_adder_network<mockturtle::names_view<technology_network>>();

    auto tec_b = network_balancing<technology_network>(tec);

    auto tec_topo = bfs_topo_view(tec_b);

    auto tec_r = mutable_rank_view(tec_topo);

    mincross_stats  st{};
    mincross_params p{};
    p.optimize = false;

    auto ntk = mincross(tec_r, p, &st);  // counts crossings
    CHECK(st.num_crossings == 4);

    p.optimize = true;
    ntk        = mincross(tec_r, p, &st);
    CHECK(st.num_crossings == 2);
}

TEST_CASE("Connected components are ordered one after the other", "[mincross]")
{
    // two independent cones whose nodes interleave in creation order
    technology_network tec{};

    const auto a1 = tec.create_pi();
    const auto b1 = tec.create_pi();
    const auto a2 = tec.create_pi();
    const auto b2 = tec.create_pi();
    const auto c1 = tec.create_pi();
    const auto c2 = tec.create_pi();

    const auto f1 = tec.create_and(a1, b1);
    const auto f2 = tec.create_and(a2, b2);
    const auto g1 = tec.create_or(b1, c1);
    const auto g2 = tec.create_or(b2, c2);

    tec.create_po(f1);
    tec.create_po(f2);
    tec.create_po(g1);
    tec.create_po(g2);

    auto tec_r = mutable_rank_view(tec);

    mincross_stats  st{};
    mincross_params p{};
    p.optimize = false;
    mincross(tec_r, p, &st);
    CHECK(st.num_crossings > 0);

    p.optimize     = true;
    const auto ntk = mincross(tec_r, p, &st);
    CHECK(st.num_crossings == 0);

    // the inputs of one component are contiguous
    const auto pos = [&ntk](const auto& s) { return ntk.rank_position(ntk.get_node(s)); };

    const auto first_min  = std::min({pos(a1), pos(b1), pos(c1)});
    const auto first_max  = std::max({pos(a1), pos(b1), pos(c1)});
    const auto second_min = std::min({pos(a2), pos(b2), pos(c2)});
    const auto second_max = std::max({pos(a2), pos(b2), pos(c2)});

    CHECK((first_max < second_min || second_max < first_min));
}

TEST_CASE("The input order is never made worse", "[mincross]")
{
    // a crossing-free order given by the caller survives, even though the breadth-first seeds have crossings
    technology_network tec{};

    const auto a = tec.create_pi();
    const auto b = tec.create_pi();
    const auto c = tec.create_pi();
    const auto d = tec.create_pi();

    const auto f1 = tec.create_and(c, d);
    const auto f2 = tec.create_and(a, b);

    tec.create_po(f1);
    tec.create_po(f2);

    auto tec_r = mutable_rank_view(tec);

    mincross_stats  st{};
    mincross_params p{};
    p.optimize = false;
    mincross(tec_r, p, &st);
    CHECK(st.num_crossings == 4);

    const std::vector<mockturtle::node<technology_network>> rank1{tec_r.get_node(f2), tec_r.get_node(f1)};
    tec_r.set_ranks(1, rank1);
    mincross(tec_r, p, &st);
    CHECK(st.num_crossings == 0);

    p.optimize     = true;
    const auto ntk = mincross(tec_r, p, &st);
    CHECK(st.num_crossings == 0);
    CHECK(ntk.rank_position(ntk.get_node(f2)) < ntk.rank_position(ntk.get_node(f1)));
}

TEST_CASE("Fixed inputs keep their order across components", "[mincross]")
{
    technology_network tec{};

    const auto a1 = tec.create_pi();
    const auto a2 = tec.create_pi();
    const auto b1 = tec.create_pi();
    const auto b2 = tec.create_pi();

    tec.create_po(tec.create_and(a1, b1));
    tec.create_po(tec.create_and(a2, b2));

    auto tec_r = mutable_rank_view(tec);

    mincross_stats  st{};
    mincross_params p{};
    p.fixed_pis    = true;
    const auto ntk = mincross(tec_r, p, &st);

    CHECK(ntk.rank_position(ntk.get_node(a1)) == 0);
    CHECK(ntk.rank_position(ntk.get_node(a2)) == 1);
    CHECK(ntk.rank_position(ntk.get_node(b1)) == 2);
    CHECK(ntk.rank_position(ntk.get_node(b2)) == 3);
    CHECK(st.num_crossings == 1);

    p.fixed_pis = false;
    mincross(tec_r, p, &st);
    CHECK(st.num_crossings == 0);
}

TEST_CASE("LUT-mapped blueprints never gain crossings", "[mincross]")
{
    const auto check = [](auto&& ntk)
    {
        const auto tec_b = network_balancing<technology_network>(ntk);
        auto       tec_r = mutable_rank_view(tec_b);

        mincross_stats  st{};
        mincross_params p{};
        p.optimize = false;
        mincross(tec_r, p, &st);
        const auto before = st.num_crossings;

        p.optimize       = true;
        const auto ntk_o = mincross(tec_r, p, &st);
        CHECK(st.num_crossings <= before);

        p.optimize = false;
        mincross(ntk_o, p, &st);
        CHECK(st.num_crossings <= before);
    };

    check(blueprints::maj4_network<technology_network>());
    check(blueprints::full_adder_network<technology_network>());
    check(blueprints::mux21_network<technology_network>());
    check(blueprints::parity_network<technology_network>());
    check(blueprints::clpl<technology_network>());
    check(blueprints::nand_xnor_network<technology_network>());
}
