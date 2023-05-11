//
// Created by benjamin on 25.04.23.
//
#include <catch2/catch_test_macros.hpp>

#include "utils/blueprints/majority_network_blueprints.hpp"
#include "utils/blueprints/network_blueprints.hpp"

#include <fiction/algorithms/physical_design/apply_gate_library.hpp>
#include <fiction/algorithms/physical_design/ortho_ordering_network.hpp>
#include <fiction/algorithms/physical_design/orthogonal_majority_network.hpp>
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
    check_colors(mockturtle::fanout_view{fanout_substitution<technology_network>(
        blueprints::maj_random_1<technology_network>())});
    check_colors(mockturtle::fanout_view{fanout_substitution<technology_network>(
        blueprints::maj1_network<technology_network>())});
    check_colors(mockturtle::fanout_view{fanout_substitution<technology_network>(
        blueprints::maj4_network<technology_network>())});
    check_colors(mockturtle::fanout_view{fanout_substitution<technology_network>(
        blueprints::maj_e_s_buf<technology_network>())});
    check_colors(mockturtle::fanout_view{fanout_substitution<technology_network>(
        blueprints::maj_mg_buf<technology_network>())});
    check_colors(mockturtle::fanout_view{fanout_substitution<technology_network>(
        blueprints::maj_one_buf_a<technology_network>())});
    check_colors(mockturtle::fanout_view{fanout_substitution<technology_network>(
        blueprints::maj_three_buf<technology_network>())});
    check_colors(mockturtle::fanout_view{fanout_substitution<technology_network>(
        blueprints::maj_two_buf<technology_network>())});
    check_colors(mockturtle::fanout_view{fanout_substitution<technology_network>(
        blueprints::maj_two_buf_a<technology_network>())});
    check_colors(mockturtle::fanout_view{fanout_substitution<technology_network>(
        blueprints::maj_two_buf_b<technology_network>())});
}

TEST_CASE("Majority Gate library application", "[ortho-majority-gate]")
{
    using gate_layout = gate_level_layout<clocked_layout<tile_based_layout<cartesian_layout<offset::ucoord_t>>>>;
    using cell_layout = cell_level_layout<qca_technology, clocked_layout<cartesian_layout<offset::ucoord_t>>>;

    const auto check_ortho_ordering = [](const auto& ntk)
    {
        orthogonal_physical_design_stats stats{};

        auto layout = orthogonal_majority_network<gate_layout>(ntk, {}, &stats);

        CHECK_NOTHROW(apply_gate_library<cell_layout, qca_one_library>(layout));
    };

    check_ortho_ordering(blueprints::maj_one_buf<mockturtle::aig_network>());
    check_ortho_ordering(blueprints::maj_random_1<mockturtle::aig_network>());
    check_ortho_ordering(blueprints::maj1_network<mockturtle::aig_network>());
    check_ortho_ordering(blueprints::maj4_network<technology_network>());
    check_ortho_ordering(blueprints::maj_e_s_buf<technology_network>());
    check_ortho_ordering(blueprints::maj_mg_buf<technology_network>());
    check_ortho_ordering(blueprints::maj_one_buf_a<mockturtle::mig_network>());
    check_ortho_ordering(blueprints::maj_three_buf<mockturtle::mig_network>());
    check_ortho_ordering(blueprints::maj_two_buf<mockturtle::mig_network>());
    check_ortho_ordering(blueprints::maj_two_buf_a<mockturtle::mig_network>());
    check_ortho_ordering(blueprints::maj_two_buf_b<mockturtle::mig_network>());

    // constant input network
    check_ortho_ordering(blueprints::unbalanced_and_inv_network<mockturtle::mig_network>());
}

TEST_CASE("one_majority_gate", "[ortho-majority-gate]")
{
    using gate_layout = gate_level_layout<clocked_layout<tile_based_layout<cartesian_layout<offset::ucoord_t>>>>;

    auto test_nw = blueprints::maj_random_5<mockturtle::names_view<technology_network>>();

    orthogonal_physical_design_stats stats{};

    const auto layout = orthogonal_majority_network<gate_layout>(test_nw, {}, &stats);

    gate_level_drvs(layout);

    // fiction::debug::write_dot_layout(layout);
}