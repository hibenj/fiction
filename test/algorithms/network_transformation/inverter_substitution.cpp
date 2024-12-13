//
// Created by benjamin on 13.04.23.
//

#include <catch2/catch_test_macros.hpp>

#include "fiction/utils/debug/network_writer.hpp"
#include "utils/blueprints/network_blueprints.hpp"
#include "utils/equivalence_checking_utils.hpp"

#include <fiction/algorithms/network_transformation/fanout_substitution.hpp>
#include <fiction/algorithms/network_transformation/inverter_substitution.hpp>
#include <fiction/networks/technology_network.hpp>

#include <mockturtle/networks/aig.hpp>
#include <mockturtle/traits.hpp>
#include <mockturtle/views/fanout_view.hpp>
#include <mockturtle/views/names_view.hpp>

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <vector>

using namespace fiction;

TEST_CASE("Minimal FO inverter substitution, [inverter-substitution]")
{
    technology_network tec{};
    const auto         pi0 = tec.create_pi();
    const auto buf0 = tec.create_buf(pi0);
    const auto inv0 = tec.create_not(buf0);
    const auto inv1 = tec.create_not(buf0);
    const auto buf1 = tec.create_buf(inv0);
    const auto buf2 = tec.create_buf(inv1);
    tec.create_po(buf1);
    tec.create_po(buf2);
    fiction::debug::write_dot_network(tec);
    auto substituted_network = inverter_substitution(tec);
    fiction::debug::write_dot_network(substituted_network, "ntk_sub");
}

TEST_CASE("Minimal FO inverter substitution with output preservation, [inverter-substitution]")
{
    technology_network tec{};
    const auto         pi0 = tec.create_pi();
    const auto buf0 = tec.create_buf(pi0);
    const auto inv0 = tec.create_not(buf0);
    const auto inv1 = tec.create_not(buf0);
    tec.create_po(inv0);
    tec.create_po(inv1);
    fiction::debug::write_dot_network(tec);
    auto substituted_network = inverter_substitution(tec);
    fiction::debug::write_dot_network(substituted_network, "ntk_sub");
}

TEST_CASE("Minimal AND/OR inverter substitution, [inverter-substitution]")
{
    technology_network tec_and{};
    const auto         pi0 = tec_and.create_pi();
    const auto         pi1 = tec_and.create_pi();
    const auto inv0 = tec_and.create_not(pi0);
    const auto inv1 = tec_and.create_not(pi1);
    const auto and0 = tec_and.create_or(inv0, inv1);
    tec_and.create_po(and0);
    detail::operation_mode mode = detail::operation_mode::AND_OR_ONLY;
    auto substituted_network = inverter_substitution(tec_and, mode);

    /*technology_network tec_or{};
    const auto         pi0_or = tec_or.create_pi();
    const auto         pi1_or = tec_or.create_pi();
    const auto inv0_or = tec_or.create_not(pi0_or);
    const auto inv1_or = tec_or.create_not(pi1_or);
    const auto and0_or = tec_or.create_or(inv0_or, inv1_or);
    // const auto buf1 = tec.create_buf(and0);
    tec_or.create_po(and0_or);
    mode = detail::operation_mode::AND_OR_ONLY;
    substituted_network = inverter_substitution(tec_or, mode);
    fiction::debug::write_dot_network(substituted_network, "ntk_sub");*/
}

TEST_CASE("Propagating AND/OR inverter substitution, [inverter-substitution]")
{
    technology_network tec{};
    const auto         pi0 = tec.create_pi();
    const auto         pi1 = tec.create_pi();
    const auto         pi2 = tec.create_pi();
    const auto inv0 = tec.create_not(pi0);
    const auto inv1 = tec.create_not(pi1);
    const auto buf0 = tec.create_buf(pi2);
    const auto inv2 = tec.create_not(buf0);
    const auto and0 = tec.create_and(inv0, inv1);
    const auto or0 = tec.create_or(and0, inv2);
    // const auto buf1 = tec.create_buf(and0);
    tec.create_po(or0);
    // fiction::debug::write_dot_network(tec);
    const detail::operation_mode mode = detail::operation_mode::AND_OR_ONLY;
    auto substituted_network = inverter_substitution(tec, mode);
    fiction::debug::write_dot_network(substituted_network, "ntk_sub");
}
