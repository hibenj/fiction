//
// Created by benjamin on 30.06.2025
//

#include <catch2/catch_test_macros.hpp>

#include "fiction/algorithms/graph/mincross.hpp"
#include "fiction/algorithms/network_transformation/network_balancing.hpp"
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

TEST_CASE("One crossing example, [gate-crossing-planarization]")
{
    technology_network tec{};

    const auto x1 = tec.create_pi();
    const auto x2 = tec.create_pi();
    const auto f1 = tec.create_and(x1, x2);
    const auto f2 = tec.create_or(x1, x2);
    tec.create_po(f1);
    tec.create_po(f2);

    auto tec_r = mutable_rank_view(tec);

    mincross_stats        st{};
    const mincross_params p{};
    auto                  ntk = mincross(tec_r, p, &st, false);  // counts crossings
    CHECK(st.num_crossings == 1);

    /*ntk.foreach_node_in_rank(1, [](const auto& n)
                             {
                                 std::cout << n << std::endl;
                             });*/

    auto ntk_new = insert_planar_crossings(ntk);

    fiction::debug::write_dot_network(ntk_new);

    CHECK(tec_r.check_validity() == 1);
}
