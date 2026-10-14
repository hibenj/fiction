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
 * @brief Tests for `fiction/synthesis/network_balancing.hpp`.
 * @author Marcel Walter (marcelwa)
 * @author Benjamin Hien (hibenj)
 */

#include <catch2/catch_test_macros.hpp>

#include "utils/blueprints/network_blueprints.hpp"
#include "utils/equivalence_checking_utils.hpp"

#include <fiction/networks/technology_network.hpp>
#include <fiction/synthesis/network_balancing.hpp>

#include <mockturtle/networks/aig.hpp>
#include <mockturtle/networks/mig.hpp>
#include <mockturtle/networks/xag.hpp>
#include <mockturtle/views/depth_view.hpp>
#include <mockturtle/views/fanout_view.hpp>

#include <cstdint>

using namespace fiction;
using namespace fiction::networks;
using namespace fiction::synthesis;

TEST_CASE("Simple network balancing", "[network-balancing]")
{
    const auto tec = blueprints::unbalanced_and_inv_network<technology_network>();

    const auto balanced = network_balancing<technology_network>(tec);

    CHECK(balanced.size() == tec.size() + 1);
    CHECK(is_balanced(balanced));

    check_eq(tec, balanced);
}

TEST_CASE("Complex network balancing", "[network-balancing]")
{
    const auto aig = blueprints::maj4_network<mockturtle::aig_network>();
    const auto mig = blueprints::maj4_network<mockturtle::mig_network>();
    const auto xag = blueprints::maj4_network<mockturtle::xag_network>();
    const auto tec = blueprints::maj4_network<technology_network>();

    SECTION("without primary output unification")
    {
        const auto balanced_aig = network_balancing<technology_network>(aig);
        const auto balanced_mig = network_balancing<technology_network>(mig);
        const auto balanced_xag = network_balancing<technology_network>(xag);
        const auto balanced_tec = network_balancing<technology_network>(tec);

        CHECK(is_balanced(balanced_aig));
        CHECK(is_balanced(balanced_mig));
        CHECK(is_balanced(balanced_xag));
        CHECK(is_balanced(balanced_tec));

        CHECK(!is_balanced(balanced_aig, {.unify_outputs = true}));
        CHECK(!is_balanced(balanced_mig, {.unify_outputs = true}));
        CHECK(!is_balanced(balanced_xag, {.unify_outputs = true}));
        CHECK(!is_balanced(balanced_tec, {.unify_outputs = true}));

        check_eq(aig, balanced_aig);
        check_eq(mig, balanced_mig);
        check_eq(xag, balanced_xag);
        check_eq(tec, balanced_tec);
    }
    SECTION("with primary output unification")
    {
        const auto balanced_aig = network_balancing<technology_network>(aig, {.unify_outputs = true});
        const auto balanced_mig = network_balancing<technology_network>(mig, {.unify_outputs = true});
        const auto balanced_xag = network_balancing<technology_network>(xag, {.unify_outputs = true});
        const auto balanced_tec = network_balancing<technology_network>(tec, {.unify_outputs = true});

        CHECK(is_balanced(balanced_aig, {.unify_outputs = true}));
        CHECK(is_balanced(balanced_mig, {.unify_outputs = true}));
        CHECK(is_balanced(balanced_xag, {.unify_outputs = true}));
        CHECK(is_balanced(balanced_tec, {.unify_outputs = true}));

        check_eq(aig, balanced_aig);
        check_eq(mig, balanced_mig);
        check_eq(xag, balanced_xag);
        check_eq(tec, balanced_tec);
    }
}

TEST_CASE("Balance a network without primary outputs", "[network-balancing]")
{
    technology_network ntk{};
    for (const bool unify_outputs : {false, true})
    {
        const auto empty = network_balancing<technology_network>(ntk, {.unify_outputs = unify_outputs});
        CHECK(empty.num_gates() == 0);
        CHECK(empty.num_pos() == 0);
    }
    ntk.create_pi();
    const auto balanced = network_balancing<technology_network>(ntk, {.unify_outputs = true});
    CHECK(balanced.num_pis() == 1);
    CHECK(balanced.num_pos() == 0);
}

TEST_CASE("Constant primary outputs are not buffered when unifying outputs", "[network-balancing]")
{
    technology_network ntk{};

    const auto x1 = ntk.create_pi();
    const auto x2 = ntk.create_pi();
    const auto a1 = ntk.create_and(x1, x2);
    const auto a2 = ntk.create_and(a1, x1);
    ntk.create_po(a2);
    ntk.create_po(ntk.get_constant(false));

    // by default, the constant output is buffered up to the output level like any other
    const auto buffered = network_balancing<technology_network>(ntk, {.unify_outputs = true});

    CHECK(is_balanced(buffered, {.unify_outputs = true}));
    CHECK(buffered.size() == ntk.size() + 1 + 2);

    const auto balanced =
        network_balancing<technology_network>(ntk, {.unify_outputs = true, .buffer_constant_outputs = false});

    CHECK(is_balanced(balanced, {.unify_outputs = true, .buffer_constant_outputs = false}));
    CHECK(!is_balanced(balanced, {.unify_outputs = true}));
    CHECK(balanced.num_pos() == 2);

    uint32_t constant_pos = 0;
    balanced.foreach_po(
        [&balanced, &constant_pos](const auto& po)
        {
            if (balanced.is_constant(balanced.get_node(po)))
            {
                ++constant_pos;
            }
        });
    CHECK(constant_pos == 1);

    // one buffer balances x1 into a2; the constant output receives none
    CHECK(balanced.size() == ntk.size() + 1);

    check_eq(ntk, balanced);
}

TEST_CASE("Shared buffer chains", "[network-balancing]")
{
    // x1 feeds a1 on level 1, a2 on level 2, and a3 on level 3: three consumers on three levels
    technology_network ntk{};

    const auto x1 = ntk.create_pi();
    const auto x2 = ntk.create_pi();
    const auto a1 = ntk.create_and(x1, x2);
    const auto a2 = ntk.create_and(a1, x1);
    const auto a3 = ntk.create_and(a2, x1);
    ntk.create_po(a3);
    ntk.create_po(a1);

    SECTION("private chains: one chain per edge")
    {
        const auto balanced = network_balancing<technology_network>(ntk);

        // a2 needs one buffer for x1, a3 needs two
        CHECK(balanced.size() == ntk.size() + 3);
        CHECK(is_balanced(balanced));
        check_eq(ntk, balanced);
    }
    SECTION("shared chains: one buffer per source and level")
    {
        const auto balanced = network_balancing<technology_network>(ntk, {.share_buffers = true});

        // the buffer that a2 reads is extended by one buffer for a3
        CHECK(balanced.size() == ntk.size() + 2);
        CHECK(is_balanced(balanced));
        check_eq(ntk, balanced);

        // the first buffer has two fanouts (a2 and the second buffer); the branch sits right below a2
        const mockturtle::fanout_view fanouts{balanced};
        const mockturtle::depth_view  depth{balanced};
        uint32_t                      buffers           = 0;
        uint32_t                      branching_buffers = 0;
        balanced.foreach_gate(
            [&balanced, &fanouts, &depth, &buffers, &branching_buffers](const auto& n)
            {
                if (balanced.is_buf(n))
                {
                    ++buffers;
                    if (fanouts.fanout_size(n) == 2)
                    {
                        ++branching_buffers;
                        CHECK(depth.level(n) == 1);
                    }
                }
            });
        CHECK(buffers == 2);
        CHECK(branching_buffers == 1);
    }
    SECTION("shared chains with unified outputs")
    {
        const auto balanced =
            network_balancing<technology_network>(ntk, {.unify_outputs = true, .share_buffers = true});

        // the output of a1 on level 3 shares the chain x1 does not need: a1 has no other consumer above level 2, so
        // its output chain is its own, two buffers long; x1's chain stays at two buffers
        CHECK(is_balanced(balanced, {.unify_outputs = true}));
        CHECK(balanced.size() == ntk.size() + 2 + 2);
        check_eq(ntk, balanced);
    }
    SECTION("shared chains with a unified complemented output")
    {
        mockturtle::aig_network aig{};
        const auto              b1 = aig.create_pi();
        const auto              b2 = aig.create_pi();
        const auto              n1 = aig.create_and(b1, b2);
        const auto              n2 = aig.create_and(n1, b1);
        aig.create_po(n2);
        aig.create_po(!n1);

        const auto balanced =
            network_balancing<technology_network>(aig, {.unify_outputs = true, .share_buffers = true});

        CHECK(is_balanced(balanced, {.unify_outputs = true}));
        check_eq(aig, balanced);
    }
}

TEST_CASE("Shared buffer chains never need more buffers", "[network-balancing]")
{
    const auto tec = blueprints::maj4_network<technology_network>();

    const auto private_chains = network_balancing<technology_network>(tec);
    const auto shared_chains  = network_balancing<technology_network>(tec, {.share_buffers = true});

    CHECK(is_balanced(shared_chains));
    CHECK(shared_chains.size() <= private_chains.size());
    CHECK(mockturtle::depth_view{shared_chains}.depth() == mockturtle::depth_view{private_chains}.depth());
    check_eq(tec, shared_chains);
}
