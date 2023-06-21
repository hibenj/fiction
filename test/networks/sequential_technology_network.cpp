//
// Created by benjamin on 02.05.23.
//

#include <catch2/catch_test_macros.hpp>

#include "utils/blueprints/network_blueprints.hpp"

#include <fiction/algorithms/network_transformation/fanout_substitution.hpp>
#include <fiction/networks/sequential_technology_network.hpp>
#include <fiction/networks/technology_network.hpp>

using namespace fiction;

TEST_CASE("check for sequential traits", "[tec_sequential]")
{
    technology_network tec{};
    // check traits of technology network
    CHECK(mockturtle::has_size_v<technology_network>);
    CHECK(mockturtle::has_get_constant_v<technology_network>);
    CHECK(mockturtle::has_is_constant_v<technology_network>);
    CHECK(mockturtle::has_is_pi_v<technology_network>);
    CHECK(mockturtle::has_is_constant_v<technology_network>);
    CHECK(mockturtle::has_get_node_v<technology_network>);
    CHECK(mockturtle::has_is_complemented_v<technology_network>);
    CHECK(mockturtle::has_foreach_po_v<technology_network>);
    CHECK(mockturtle::has_create_po_v<technology_network>);
    CHECK(mockturtle::has_create_pi_v<technology_network>);
    CHECK(mockturtle::has_create_and_v<technology_network>);
    // check traits of sequential technology network
    CHECK(!has_create_ro_v<technology_network>);
    CHECK(!has_create_ri_v<technology_network>);
    CHECK(!mockturtle::has_ri_to_ro_v<technology_network>);
    CHECK(!mockturtle::has_ro_to_ri_v<technology_network>);
    CHECK(!mockturtle::has_create_ro_v<technology_network>);
    CHECK(!mockturtle::has_create_ri_v<technology_network>);
    CHECK(!mockturtle::has_foreach_ro_v<technology_network>);
    CHECK(!mockturtle::has_foreach_ri_v<technology_network>);

    mockturtle::sequential<technology_network> tec_seq;
    // check traits of technology network
    CHECK(mockturtle::has_size_v<mockturtle::sequential<technology_network>>);
    CHECK(mockturtle::has_get_constant_v<mockturtle::sequential<technology_network>>);
    CHECK(mockturtle::has_is_constant_v<mockturtle::sequential<technology_network>>);
    CHECK(mockturtle::has_is_pi_v<mockturtle::sequential<technology_network>>);
    CHECK(mockturtle::has_is_constant_v<mockturtle::sequential<technology_network>>);
    CHECK(mockturtle::has_get_node_v<mockturtle::sequential<technology_network>>);
    CHECK(mockturtle::has_is_complemented_v<mockturtle::sequential<technology_network>>);
    CHECK(mockturtle::has_foreach_po_v<mockturtle::sequential<technology_network>>);
    CHECK(mockturtle::has_create_po_v<mockturtle::sequential<technology_network>>);
    CHECK(mockturtle::has_create_pi_v<mockturtle::sequential<technology_network>>);
    CHECK(mockturtle::has_create_and_v<mockturtle::sequential<technology_network>>);
    // check traits of sequential technology network
    CHECK(has_create_ro_v<mockturtle::sequential<technology_network>>);
    CHECK(has_create_ri_v<mockturtle::sequential<technology_network>>);
    CHECK(mockturtle::has_ri_to_ro_v<mockturtle::sequential<technology_network>>);
    CHECK(mockturtle::has_ro_to_ri_v<mockturtle::sequential<technology_network>>);
    CHECK(mockturtle::has_create_ro_v<mockturtle::sequential<technology_network>>);
    CHECK(mockturtle::has_create_ri_v<mockturtle::sequential<technology_network>>);
    CHECK(mockturtle::has_foreach_ro_v<mockturtle::sequential<technology_network>>);
    CHECK(mockturtle::has_foreach_ri_v<mockturtle::sequential<technology_network>>);
}

TEST_CASE("create and use sequential functions", "[tec_sequential]")
{
    mockturtle::sequential<technology_network> tec_seq;
    // check traits of technology network
    CHECK(mockturtle::has_foreach_po_v<mockturtle::sequential<technology_network>>);
    CHECK(mockturtle::has_create_po_v<mockturtle::sequential<technology_network>>);
    CHECK(mockturtle::has_create_pi_v<mockturtle::sequential<technology_network>>);
    CHECK(mockturtle::has_create_and_v<mockturtle::sequential<technology_network>>);
    // check traits of sequential technology network
    CHECK(has_create_ro_v<mockturtle::sequential<technology_network>>);
    CHECK(has_create_ri_v<mockturtle::sequential<technology_network>>);

    const auto x1 = tec_seq.create_pi();
    const auto x2 = tec_seq.create_pi();
    const auto x3 = tec_seq.create_pi();

    // two constant nodes and three PIs
    CHECK( tec_seq.size() == 5 );
    CHECK( tec_seq.num_registers() == 0 );
    CHECK( tec_seq.num_pis() == 3 );
    CHECK( tec_seq.num_pos() == 0 );

    const auto f1 = tec_seq.create_and( x1, x2 );
    tec_seq.create_po( f1 );
    tec_seq.create_po( !f1 );

    const auto f2 = tec_seq.create_and( f1, x3 );
    const auto ro = tec_seq.create_ro();
    tec_seq.create_po( ro );
    tec_seq.create_ri( f2 );

    CHECK( tec_seq.num_pos() == 3 );
    CHECK( tec_seq.num_registers() == 1 );

    tec_seq.foreach_po( [&]( auto s , auto i) {
                       switch ( i )
                       {
                           case 0:
                               CHECK( s == f1 );
                               break;
                           case 1:
                               CHECK( s == !f1 );
                               break;
                           case 2:
                               // Check if the output (connected to the register) data is the same as the node data being registered.
                               CHECK( s == ro );
                               break;
                           default:
                               CHECK( false );
                               break;
                       }
                   } );

    tec_seq.foreach_ri( [&]( auto s , auto i) {
                           switch ( i )
                           {
                               case 0:
                                   CHECK( s == f2 );
                                   break;
                               default:
                                   CHECK( false );
                                   break;
                           }
                       } );
}
