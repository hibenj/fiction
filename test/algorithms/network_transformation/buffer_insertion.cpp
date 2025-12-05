//
// Created by benjamin on 3.12.25.
//

#include <catch2/catch_test_macros.hpp>

#include "fiction/networks/technology_network.hpp"
#include <fiction/utils/debug/network_writer.hpp>

#include <lorina/lorina.hpp>
#include <mockturtle/algorithms/aqfp/buffer_insertion.hpp>
#include <mockturtle/algorithms/aqfp/buffer_verification.hpp>
#include <mockturtle/io/aiger_reader.hpp>
#include <mockturtle/networks/aig.hpp>
#include <mockturtle/networks/buffered.hpp>
#include <mockturtle/networks/mig.hpp>
#include <mockturtle/traits.hpp>

using namespace mockturtle;

TEST_CASE( "various assumptions", "[buffer-insertion]" )
{
    aig_network aig;
    auto const a = aig.create_pi();
    auto const b = aig.create_pi();
    auto const c = aig.create_pi();
    auto const d = aig.create_pi();
    auto const e = aig.create_pi();

    auto const f1 = aig.create_and( c, d );
    auto const f2 = aig.create_or( c, d );
    auto const f3 = aig.create_and( d, e );
    auto const f4 = aig.create_and( f2, f3 );

    aig.create_po( aig.get_constant( false ) ); // const -- PO
    aig.create_po( a );                         // PI -- PO
    aig.create_po( b );
    aig.create_po( b );
    aig.create_po( b ); // PI -- buffer tree -- PO
    aig.create_po( f1 );
    aig.create_po( f3 );
    aig.create_po( f4 );

    aqfp_assumptions_realistic asp;
    asp.splitter_capacity = 2u;
    asp.num_phases = 1u;
    asp.ci_phases = {0};

    buffer_insertion_params ps;
    ps.scheduling = buffer_insertion_params::ASAP;
    ps.optimization_effort = buffer_insertion_params::none;

    /* branch PI, balance PI and PO */
    asp.ci_capacity = 1;
    asp.balance_cios = true;
    ps.assume = asp;
    {
        buffer_insertion buffering( aig, ps );
        buffered_aig_network buffered;
        CHECK( buffering.run( buffered ) == 23u );
        CHECK( verify_aqfp_buffer( buffered, asp, buffering.pi_levels() ) == true );
    }

    /* branch PI, balance neither */
    asp.ci_capacity = 1;
    asp.balance_cios = false;
    ps.assume = asp;
    {
        ps.scheduling = buffer_insertion_params::ASAP;
        buffer_insertion buffering1( aig, ps );
        buffered_aig_network buffered1;
        buffering1.run( buffered1 );
        CHECK( verify_aqfp_buffer( buffered1, asp, buffering1.pi_levels() ) == true );

        ps.scheduling = buffer_insertion_params::ALAP;
        buffer_insertion buffering2( aig, ps );
        buffered_aig_network buffered2;
        buffering2.run( buffered2 );
        CHECK( verify_aqfp_buffer( buffered2, asp, buffering2.pi_levels() ) == true );

        ps.scheduling = buffer_insertion_params::ASAP_depth;
        buffer_insertion buffering3( aig, ps );
        buffered_aig_network buffered3;
        buffering3.run( buffered3 );
        CHECK( buffering3.depth() == 4 );
        CHECK( verify_aqfp_buffer( buffered3, asp, buffering3.pi_levels() ) == true );

        ps.scheduling = buffer_insertion_params::ALAP_depth;
        buffer_insertion buffering4( aig, ps );
        buffered_aig_network buffered4;
        buffering4.run( buffered4 );
        CHECK( buffering4.depth() == 4 );
        CHECK( verify_aqfp_buffer( buffered4, asp, buffering4.pi_levels() ) == true );
    }
}

using namespace fiction;

TEST_CASE( "Try technology network", "[buffer-insertion]")
{
    technology_network tec{};

    const auto pi1 = tec.create_pi();
    const auto pi2 = tec.create_pi();
    const auto n1 = tec.create_not(pi1);
    const auto a1 = tec.create_and(n1, pi2);
    tec.create_po(a1);

    buffer_insertion_params ps;
    ps.scheduling = buffer_insertion_params::ASAP;
    ps.optimization_effort = buffer_insertion_params::none;

    aqfp_assumptions_realistic asp;
    asp.splitter_capacity = 2u;
    asp.num_phases = 1u;
    asp.ci_phases = {0};

    asp.ci_capacity = 1;
    asp.balance_cios = false;
    ps.assume = asp;

    technology_network tec_buf{};

    buffer_insertion buffering( tec, ps );
    /*const auto num_bufs = buffering.run( tec_buf );

    std::cout << "num_bufs: " << num_bufs << '\n';

    debug::write_dot_network(tec);
    debug::write_dot_network(tec_buf, "tec_buf");*/
}