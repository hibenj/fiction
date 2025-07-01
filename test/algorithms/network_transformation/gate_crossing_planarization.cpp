//
// Created by benjamin on 30.06.2025
//

#include <catch2/catch_test_macros.hpp>

#include "fiction/algorithms/network_transformation/network_balancing.hpp"
#include "fiction/algorithms/properties/count_gate_types.hpp"
#include "fiction/algorithms/verification/virtual_miter.hpp"
#include "fiction/networks/views/mutable_rank_view.hpp"
#include "fiction/utils/debug/network_writer.hpp"
#include "utils/blueprints/network_blueprints.hpp"

#include <fiction/algorithms/network_transformation/gate_crossing_planarization.hpp>
#include <fiction/networks/technology_network.hpp>

#include <mockturtle/algorithms/equivalence_checking.hpp>

#include <cassert>
#include <cstdlib>

using namespace fiction;

TEST_CASE("Minimal FO inverter substitution, [gate-crossing-planarization]")
{
    const auto ntk = blueprints::half_adder_network<technology_network>();
    const auto ntk_blc = network_balancing<technology_network>(ntk);

    fiction::debug::write_dot_network(ntk_blc);
}