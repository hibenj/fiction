//
// Created by benjamin on 09.12.25.
//

#include <catch2/catch_test_macros.hpp>

#include "fiction/networks/technology_network.hpp"
#include "fiction/networks/views/mutable_rank_view.hpp"
#include "fiction/utils/debug/network_writer.hpp"

#include <fiction/algorithms/network_transformation/planarization.hpp>

#include <mockturtle/algorithms/equivalence_checking.hpp>
#include <mockturtle/algorithms/miter.hpp>
#include <mockturtle/traits.hpp>

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <utility>
#include <vector>

using namespace fiction;

TEST_CASE("Hello World", "[planarization]")
{
    technology_network tec{};
    const auto         pi1 = tec.create_pi();
    const auto         pi2 = tec.create_pi();
    const auto         pi3 = tec.create_pi();
    const auto         a1  = tec.create_and(pi1, pi2);
    const auto         o1  = tec.create_or(pi1, pi2);
    const auto         a2  = tec.create_and(pi3, pi1);
    tec.create_po(a1);
    tec.create_po(o1);
    tec.create_po(a2);

    debug::write_dot_network(tec);

    auto       tec_r = mutable_rank_view(tec);
    const auto tec_p = planarization(tec_r);

    debug::write_dot_network(tec_p, "planarized");
}
