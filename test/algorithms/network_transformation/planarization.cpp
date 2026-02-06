//
// Created by benjamin on 09.12.25.
//

#include <catch2/catch_test_macros.hpp>

#include "fiction/networks/technology_network.hpp"
#include "fiction/networks/views/mutable_rank_view.hpp"
#include "fiction/utils/debug/network_writer.hpp"
#include "fiction/algorithms/graph/mincross.hpp"

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
    const auto         pi1  = tec.create_pi();
    const auto         pi2  = tec.create_pi();

    const auto         pi3  = tec.create_pi();
    const auto         buf1 = tec.create_buf(pi1);
    const auto         buf2 = tec.create_buf(pi2);
    const auto         buf3 = tec.create_buf(pi3);
    const auto         buf4 = tec.create_buf(buf1);
    const auto         buf5 = tec.create_buf(buf2);
    const auto         buf6 = tec.create_buf(buf3);
    const auto         buf7 = tec.create_buf(buf1);
    const auto         a1   = tec.create_and(buf4, buf5);
    const auto         o1   = tec.create_or(buf4, buf5);
    const auto         a2   = tec.create_and(buf6, buf7);
    tec.create_po(a1);
    tec.create_po(o1);
    tec.create_po(a2);

    mincross_stats  st_min{};
    mincross_params p_min{};
    p_min.optimize = true;

    debug::write_dot_network(tec);

    auto       tec_r   = mutable_rank_view(tec);
    auto       ntk_min = mincross(tec_r, p_min, &st_min);  // counts crossings

    std::cout << "Crossings after planarization: " << st_min.num_crossings << "\n";

    const auto tec_p   = planarization(ntk_min);

    debug::write_dot_network(tec_p, "planarized");
}
