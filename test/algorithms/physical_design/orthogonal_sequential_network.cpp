//
// Created by benjamin on 02.05.23.
//
#include <catch2/catch_test_macros.hpp>

#include <fiction/utils/debug/network_writer.hpp>
#include "utils/blueprints/network_blueprints.hpp"

#include <fiction/algorithms/physical_design/orthogonal_sequential_network.hpp>

#include <iostream>

using namespace fiction;

TEST_CASE("sequential dot", "[ortho_sequential]")
{
    using gate_layout = gate_level_layout<clocked_layout<tile_based_layout<cartesian_layout<offset::ucoord_t>>>>;
    //cube::coord_t

    auto mux21 = blueprints::seq_one<mockturtle::names_view<mockturtle::sequential<technology_network>>>();
    //mux21.set_network_name("mux21");
    orthogonal_physical_design_stats stats{};

    auto layout = orthogonal_sequential_network<gate_layout>(mux21, {}, &stats);


    fiction::debug::write_dot_layout(layout);

    /*const auto cell_level_lyt = apply_gate_library<qca_cell_clk_lyt, qca_one_library>(layout);

    write_qca_layout_svg(cell_level_lyt, "TEST_maj_two_buf_cell_lvl_lyt.svg");*/

    std::cout << "PROCESSING SEQ_NW "<< std::endl;
    std::cout << "Size: " <<(stats.x_size) * (stats.y_size) << std::endl;
    std::cout << "w: " <<(stats.x_size)<< std::endl;
    std::cout << "h: " <<(stats.y_size) << std::endl;
    std::cout << "Time: " << mockturtle::to_seconds(stats.time_total) << std::endl;
    std::cout << "Num Gates: "  << stats.num_gates << std::endl;
    std::cout << "Num Wires: " << stats.num_wires<< std::endl;

    gate_level_drvs(layout);

}