/*
 * Copyright (c) 2018 - 2023 Marcel Walter
 * Copyright (c) 2023 - present Chair for Design Automation, Technical University of Munich
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

/**
 * @file
 * @brief Tests for `fiction/synthesis/planar_cell_insertion.hpp`.
 * @author Benjamin Hien (hibenj)
 */

#include <catch2/catch_test_macros.hpp>

#include <fiction/networks/technology_network.hpp>
#include <fiction/synthesis/io/planar_cell_database_json.hpp>
#include <fiction/synthesis/planar_cell_database.hpp>
#include <fiction/synthesis/planar_cell_insertion.hpp>

#include <kitty/constructors.hpp>
#include <kitty/dynamic_truth_table.hpp>
#include <kitty/operations.hpp>
#include <mockturtle/algorithms/simulation.hpp>
#include <mockturtle/networks/xag.hpp>
#include <mockturtle/views/depth_view.hpp>

#include <cstddef>
#include <cstdint>
#include <sstream>
#include <vector>

using namespace fiction::networks;
using namespace fiction::synthesis;

namespace
{

template <typename Ntk>
kitty::dynamic_truth_table simulate(const Ntk& ntk, const uint32_t num_vars)
{
    return mockturtle::simulate<kitty::dynamic_truth_table>(
               ntk, mockturtle::default_simulator<kitty::dynamic_truth_table>{num_vars})
        .front();
}

}  // namespace

TEST_CASE("A planar cell is inserted with its inverters as nodes", "[planar-cell-insertion]")
{
    planar_cell_database_params ps{};
    ps.num_inputs = 3;
    const auto db = generate_planar_cells(ps);

    technology_network ntk{};
    const auto         a = ntk.create_pi();
    const auto         b = ntk.create_pi();
    const auto         c = ntk.create_pi();

    // ~(a & b) ^ c: a cell of two gates, the negations as inverter nodes
    kitty::dynamic_truth_table ta{3};
    kitty::dynamic_truth_table tb{3};
    kitty::dynamic_truth_table tc{3};
    kitty::create_nth_var(ta, 0);
    kitty::create_nth_var(tb, 1);
    kitty::create_nth_var(tc, 2);
    const auto f = ~(ta & tb) ^ tc;

    const auto inst = insert_planar_cell(ntk, db, f, {a, b, c});
    REQUIRE(inst.has_value());
    ntk.create_po(inst->output);

    CHECK(simulate(ntk, 3) == f);
    CHECK(inst->cell.num_gates() == 2);
    CHECK(inst->gates.size() == inst->cell.depth());
    std::size_t gates = 0;
    for (const auto& level : inst->gates)
    {
        gates += level.size();
    }
    CHECK(gates == inst->cell.num_gates());
    CHECK(inst->inverters.size() == inst->cell.num_inversions());
    CHECK(ntk.num_gates() == gates + inst->inverters.size());
    for (const auto n : inst->inverters)
    {
        CHECK(ntk.is_inv(n));
    }

    // a function of a smaller support uses the smaller cell with the right fanins
    technology_network two{};
    const auto         x     = two.create_pi();
    const auto         y     = two.create_pi();
    const auto         z     = two.create_pi();
    const auto         g     = ta | ~tc;
    const auto         small = insert_planar_cell(two, db, g, {x, y, z});
    REQUIRE(small.has_value());
    two.create_po(small->output);
    CHECK(simulate(two, 3) == g);
    CHECK(small->cell.num_inputs == 2);
    CHECK(small->cell.num_gates() == 1);

    // constants and single variables need no gate
    technology_network trivial{};
    const auto         p   = trivial.create_pi();
    const auto         q   = trivial.create_pi();
    const auto         one = insert_planar_cell(trivial, db, ~kitty::dynamic_truth_table{2}, {p, q});
    REQUIRE(one.has_value());
    CHECK(trivial.is_constant(trivial.get_node(one->output)));
    CHECK(one->gates.empty());
    kitty::dynamic_truth_table tq{2};
    kitty::create_nth_var(tq, 1);
    const auto not_q = insert_planar_cell(trivial, db, ~tq, {p, q});
    REQUIRE(not_q.has_value());
    CHECK(not_q->inverters.size() == 1);
    CHECK(trivial.num_gates() == 1);
    trivial.create_po(not_q->output);
    CHECK(simulate(trivial, 2) == ~tq);
}

TEST_CASE("Every function with a cell instantiates to its truth table", "[planar-cell-insertion]")
{
    // three levels: 1070 of the 2191 four-input classes have a cell, the others report it
    planar_cell_database_params ps{};
    ps.num_inputs = 4;
    ps.max_levels = 3;
    const auto db = generate_planar_cells(ps);

    uint32_t with_cell = 0;
    uint32_t without   = 0;

    kitty::dynamic_truth_table f{4};
    do  // NOLINT(cppcoreguidelines-avoid-do-while): kitty enumerates functions until the table wraps to zero
    {
        const auto ntk = planar_cell_network<technology_network>(db, f);
        if (ntk.has_value())
        {
            ++with_cell;
            CHECK(simulate(*ntk, 4) == f);
        }
        else
        {
            ++without;
        }
        kitty::next_inplace(f);
    } while (!kitty::is_const0(f));

    // 64594 functions depend on all four inputs, 30266 of them have a cell; every other function has one
    CHECK(with_cell + without == 65536);
    CHECK(without == 64594 - 30266);
}

TEST_CASE("Cells from a JSON database insert into an XAG with complemented signals", "[planar-cell-insertion]")
{
    planar_cell_database_params ps{};
    ps.num_inputs               = 3;
    ps.gate_set                 = {planar_gate::AND, planar_gate::XOR};
    const auto        generated = generate_planar_cells(ps);
    std::stringstream stream{};
    io::write_planar_cell_database(generated, stream, &ps);
    const auto db = io::read_planar_cell_database(stream);

    // an XAG carries the inversion on the signal: no inverter nodes, the function still matches
    kitty::dynamic_truth_table f{3};
    do  // NOLINT(cppcoreguidelines-avoid-do-while): kitty enumerates functions until the table wraps to zero
    {
        const auto ntk = planar_cell_network<mockturtle::xag_network>(db, f);
        REQUIRE(ntk.has_value());
        CHECK(simulate(*ntk, 3) == f);
        kitty::next_inplace(f);
    } while (!kitty::is_const0(f));

    mockturtle::xag_network    xag{};
    const auto                 a = xag.create_pi();
    const auto                 b = xag.create_pi();
    const auto                 c = xag.create_pi();
    kitty::dynamic_truth_table ta{3};
    kitty::dynamic_truth_table tb{3};
    kitty::dynamic_truth_table tc{3};
    kitty::create_nth_var(ta, 0);
    kitty::create_nth_var(tb, 1);
    kitty::create_nth_var(tc, 2);
    const auto inst = insert_planar_cell(xag, db, ~(ta & tb) ^ tc, {a, b, c});
    REQUIRE(inst.has_value());
    CHECK(inst->inverters.empty());
    CHECK(xag.num_gates() == inst->cell.num_gates());

    // absorbing inversions changes nothing in a network without arbitrary node functions
    planar_cell_insertion_params ips{};
    ips.absorb_inversions = true;
    mockturtle::xag_network other{};
    const auto              x        = other.create_pi();
    const auto              y        = other.create_pi();
    const auto              z        = other.create_pi();
    const auto              absorbed = insert_planar_cell(other, db, ~(ta & tb) ^ tc, {x, y, z}, ips);
    REQUIRE(absorbed.has_value());
    CHECK(absorbed->inverters.empty());
    CHECK(other.num_gates() == inst->cell.num_gates());
}

TEST_CASE("Inversions are absorbed into the gates", "[planar-cell-insertion]")
{
    planar_cell_database_params ps{};
    ps.num_inputs = 3;
    const auto db = generate_planar_cells(ps);

    planar_cell_insertion_params ips{};
    ips.absorb_inversions = true;

    kitty::dynamic_truth_table ta{3};
    kitty::dynamic_truth_table tb{3};
    kitty::dynamic_truth_table tc{3};
    kitty::create_nth_var(ta, 0);
    kitty::create_nth_var(tb, 1);
    kitty::create_nth_var(tc, 2);

    // ~(a & b) ^ c: the same two gates as with inverter nodes, but no inverter
    technology_network ntk{};
    const auto         a    = ntk.create_pi();
    const auto         b    = ntk.create_pi();
    const auto         c    = ntk.create_pi();
    const auto         f    = ~(ta & tb) ^ tc;
    const auto         inst = insert_planar_cell(ntk, db, f, {a, b, c}, ips);
    REQUIRE(inst.has_value());
    ntk.create_po(inst->output);
    CHECK(simulate(ntk, 3) == f);
    CHECK(inst->cell.num_gates() == 2);
    CHECK(inst->inverters.empty());
    CHECK(ntk.num_gates() == 2);
    ntk.foreach_gate([&ntk](const auto& n) { CHECK(!ntk.is_inv(n)); });

    // a single gate with its negations becomes the two-input function the library has
    kitty::dynamic_truth_table tx{2};
    kitty::dynamic_truth_table ty{2};
    kitty::create_nth_var(tx, 0);
    kitty::create_nth_var(ty, 1);
    technology_network two{};
    const auto         x    = two.create_pi();
    const auto         y    = two.create_pi();
    const auto         nand = insert_planar_cell(two, db, ~(tx & ty), {x, y}, ips);
    REQUIRE(nand.has_value());
    CHECK(two.num_gates() == 1);
    CHECK(two.is_nand(two.get_node(nand->output)));
    // mockturtle's naming: `lt` is a & ~b, `gt` is ~a & b, `le` is a | ~b, `ge` is ~a | b
    const auto gt = insert_planar_cell(two, db, ~tx & ty, {x, y}, ips);
    REQUIRE(gt.has_value());
    CHECK(two.is_gt(two.get_node(gt->output)));
    const auto le = insert_planar_cell(two, db, tx | ~ty, {x, y}, ips);
    REQUIRE(le.has_value());
    CHECK(two.is_le(two.get_node(le->output)));
    const auto xnor = insert_planar_cell(two, db, ~(tx ^ ty), {x, y}, ips);
    REQUIRE(xnor.has_value());
    CHECK(two.is_xnor(two.get_node(xnor->output)));
    CHECK(two.num_gates() == 4);

    // the NOT of one variable has no gate to absorb into
    const auto not_x = insert_planar_cell(two, db, ~tx, {x, y}, ips);
    REQUIRE(not_x.has_value());
    CHECK(not_x->inverters.size() == 1);
    CHECK(two.is_inv(two.get_node(not_x->output)));

    // every function of three inputs: the right table, and inverters only for the three negated variables
    uint64_t                   inverters = 0;
    kitty::dynamic_truth_table g{3};
    do  // NOLINT(cppcoreguidelines-avoid-do-while): kitty enumerates functions until the table wraps to zero
    {
        const auto cell = planar_cell_network<technology_network>(db, g, ips);
        REQUIRE(cell.has_value());
        CHECK(simulate(*cell, 3) == g);
        cell->foreach_gate(
            [&cell, &inverters](const auto& n)
            {
                if (cell->is_inv(n))
                {
                    ++inverters;
                }
            });
        kitty::next_inplace(g);
    } while (!kitty::is_const0(g));
    CHECK(inverters == 3);
}
