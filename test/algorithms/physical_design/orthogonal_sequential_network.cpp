//
// Created by benjamin on 02.05.23.
//
#include <catch2/catch_test_macros.hpp>

#include "fiction/algorithms/physical_design/apply_gate_library.hpp"
#include "fiction/io/write_svg_layout.hpp"
#include "fiction/technology/qca_one_library.hpp"
#include "utils/blueprints/network_blueprints.hpp"
#include "utils/blueprints/sequential_network_blueprints.hpp"

#include <fiction/algorithms/physical_design/orthogonal_sequential_network.hpp>
#include <fiction/utils/debug/network_writer.hpp>

#include <iostream>

using namespace fiction;

TEST_CASE("Sequential circuit library application")
{
    using gate_layout = gate_level_layout<clocked_layout<tile_based_layout<cartesian_layout<offset::ucoord_t>>>>;
    using cell_layout = cell_level_layout<qca_technology, clocked_layout<cartesian_layout<offset::ucoord_t>>>;

    const auto check_sequential_library = [](const auto& ntk)
    {
        orthogonal_physical_design_stats stats{};

        auto layout = orthogonal_sequential_network<gate_layout>(ntk, {}, &stats);

        CHECK_NOTHROW(apply_gate_library<cell_layout, qca_one_library>(layout));

        gate_level_drv_stats  st = {};
        gate_level_drv_params ps = {};
        std::stringstream     ss{};
        ps.out = &ss;
        gate_level_drvs(layout, ps, &st);

        CHECK(st.drvs == 0);
    };
    // ascending number of registers
    check_sequential_library(blueprints::seq_one<mockturtle::sequential<mockturtle::aig_network>>());
    check_sequential_library(blueprints::seq_two<mockturtle::sequential<mockturtle::aig_network>>());
    check_sequential_library(blueprints::seq_three<mockturtle::sequential<mockturtle::aig_network>>());
    check_sequential_library(blueprints::seq_four<mockturtle::sequential<mockturtle::aig_network>>());
    check_sequential_library(blueprints::seq_five<mockturtle::sequential<mockturtle::aig_network>>());

    // itc99-poli
    check_sequential_library(blueprints::i99t_b01<mockturtle::sequential<mockturtle::aig_network>>());
    check_sequential_library(blueprints::i99t_b02<mockturtle::sequential<mockturtle::aig_network>>());
    check_sequential_library(blueprints::i99t_b03<mockturtle::sequential<mockturtle::aig_network>>());
    check_sequential_library(blueprints::i99t_b06<mockturtle::sequential<mockturtle::aig_network>>());

    // constant input network
    check_sequential_library(blueprints::unbalanced_and_inv_network<mockturtle::aig_network>());
}

TEST_CASE("sequential dot", "[ortho_sequential]")
{
    using gate_layout = gate_level_layout<clocked_layout<tile_based_layout<cartesian_layout<offset::ucoord_t>>>>;

    auto mux21 = blueprints::seq_five<mockturtle::names_view<mockturtle::sequential<mockturtle::aig_network>>>();

    orthogonal_physical_design_stats stats{};

    auto layout = orthogonal_sequential_network<gate_layout>(mux21, {}, &stats);

    fiction::debug::write_dot_layout(layout);

    const auto cell_level_lyt = apply_gate_library<qca_cell_clk_lyt, qca_one_library>(layout);

    write_qca_layout_svg(cell_level_lyt, "one_reg.svg");

    gate_level_drvs(layout);
}
