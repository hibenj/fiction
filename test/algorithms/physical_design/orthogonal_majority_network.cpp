//
// Created by benjamin on 25.04.23.
//
#include <catch2/catch_test_macros.hpp>
#include "utils/blueprints/network_blueprints.hpp"

#include <fiction/algorithms/physical_design/orthogonal_majority_network.hpp>
#include <fiction/algorithms/physical_design/apply_gate_library.hpp>
#include <fiction/algorithms/physical_design/ortho_ordering_network.hpp>
#include <fiction/technology/qca_one_library.hpp>
#include <fiction/utils/debug/network_writer.hpp>

#include <iostream>

using namespace fiction;

TEST_CASE("color_majority_gates", "[ortho-majority-gate]")
{
    const auto check_colors = [](const auto& ntk)
    {
        auto container = detail::east_south_edge_coloring(ntk);
        CHECK(detail::is_east_south_colored(container.color_ntk));
    };

    check_colors(mockturtle::fanout_view{fanout_substitution<technology_network>(
        blueprints::maj_one_buf<technology_network>())});
}

TEST_CASE("one_majority_gate", "[ortho-majority-gate]")
{
    using gate_layout = gate_level_layout<clocked_layout<tile_based_layout<cartesian_layout<offset::ucoord_t>>>>;

    auto test_nw = blueprints::TEST_maj_two_pluslogic_buf<mockturtle::names_view<technology_network>>();

    orthogonal_physical_design_stats stats{};

    const auto layout = orthogonal_majority_network<gate_layout>(test_nw, {}, &stats);

    gate_level_drvs(layout);

    fiction::debug::write_dot_layout(layout);
}