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
 * @brief Tests for `fiction/synthesis/planar_cell_database.hpp`.
 * @author Benjamin Hien (hibenj)
 */

#include <catch2/catch_test_macros.hpp>

#include <fiction/synthesis/planar_cell_database.hpp>

#include <kitty/bit_operations.hpp>
#include <kitty/constructors.hpp>
#include <kitty/dynamic_truth_table.hpp>
#include <kitty/operations.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

using namespace fiction::synthesis;

namespace
{

/**
 * The function an instance computes, expanded to the variables of the function it was looked up for.
 */
kitty::dynamic_truth_table expand(const planar_cell_database::instance& inst, const uint32_t num_vars)
{
    const auto                 word = inst.cell.function();
    kitty::dynamic_truth_table tt{num_vars};
    for (uint64_t m = 0; m < tt.num_bits(); ++m)
    {
        uint64_t cell_minterm = 0;
        for (std::size_t i = 0; i < inst.support.size(); ++i)
        {
            cell_minterm |= ((m >> inst.support[i]) & 1ull) << i;
        }
        if ((word >> cell_minterm) & 1ull)
        {
            kitty::set_bit(tt, m);
        }
    }
    return tt;
}

/**
 * Checks every function of `num_vars` variables against the database.
 */
void check_all_functions(const planar_cell_database& db, const uint32_t num_vars)
{
    kitty::dynamic_truth_table f{num_vars};
    do  // NOLINT(cppcoreguidelines-avoid-do-while): kitty enumerates functions until the table wraps to zero
    {
        const auto inst = db.lookup(f);
        REQUIRE(inst.has_value());
        CHECK(expand(*inst, num_vars) == f);
        kitty::next_inplace(f);
    } while (!kitty::is_const0(f));
}

}  // namespace

TEST_CASE("Negation classes", "[planar-cell-database]")
{
    using fiction::synthesis::detail::canonize;
    using fiction::synthesis::detail::tt_word;

    // every function reachable by negating inputs or the output has the same representative
    const auto full = tt_word::full(3);
    for (uint64_t f = 0; f <= full; ++f)
    {
        const auto cls = canonize(f, 3);
        CHECK(cls.representative <= f);
        CHECK(cls.representative <= (~f & full));

        auto image = tt_word::flip_all(f, cls.input_mask, 3);
        if (cls.output_negated)
        {
            image = ~image & full;
        }
        CHECK(image == cls.representative);

        for (uint32_t mask = 0; mask < 8; ++mask)
        {
            CHECK(canonize(tt_word::flip_all(f, mask, 3), 3).representative == cls.representative);
            CHECK(canonize(~tt_word::flip_all(f, mask, 3) & full, 3).representative == cls.representative);
        }
    }

    // the support test
    CHECK(tt_word::has_full_support(0x8u, 2));   // a and b
    CHECK(!tt_word::has_full_support(0xAu, 2));  // a
    CHECK(tt_word::has_full_support(0x6996u, 4));
    CHECK(!tt_word::has_full_support(0x6666u, 4));
}

TEST_CASE("A cell computes the function of its description", "[planar-cell-database]")
{
    // maj(a, ~b, c) with the output inverted
    planar_cell cell{};
    cell.num_inputs = 3;
    cell.nodes.push_back({{0, 1, 2}, 1, planar_gate::MAJ, 0b010});
    cell.output_inverted = true;

    kitty::dynamic_truth_table a{3};
    kitty::dynamic_truth_table b{3};
    kitty::dynamic_truth_table c{3};
    kitty::create_nth_var(a, 0);
    kitty::create_nth_var(b, 1);
    kitty::create_nth_var(c, 2);

    CHECK(cell.truth_table() == ~kitty::ternary_majority(a, ~b, c));
    CHECK(cell.num_gates() == 1);
    CHECK(cell.num_inversions() == 2);
    CHECK(cell.cost() == 3);
    CHECK(cell.depth() == 1);
    CHECK(cell.num_edges() == 3);
    CHECK(cell.to_string() == "maj(0,~1,2)@1 ~");

    // xor(and(a, b), c) with a wire from c past level 1
    planar_cell two{};
    two.num_inputs = 3;
    two.nodes.push_back({{0, 1}, 1, planar_gate::AND, 0});
    two.nodes.push_back({{3, 2}, 2, planar_gate::XOR, 0});
    CHECK(two.truth_table() == ((a & b) ^ c));
    CHECK(two.depth() == 2);
    CHECK(planar_cell::cheaper(cell, two) == false);  // cost 3 vs 2
    CHECK(planar_cell::cheaper(two, cell));
}

TEST_CASE("Every function of up to three inputs has a planar cell", "[planar-cell-database]")
{
    planar_cell_database_params ps{};
    ps.num_inputs = 3;

    planar_cell_database_stats st{};
    const auto                 db = generate_planar_cells(ps, &st);

    REQUIRE(st.tables.size() == 2);
    CHECK(st.tables[0].num_inputs == 2);
    CHECK(st.tables[0].classes == 2);  // and-type and xor-type
    CHECK(st.tables[0].covered == 2);
    CHECK(st.tables[1].num_inputs == 3);
    CHECK(st.tables[1].classes > 0);
    CHECK(st.tables[1].covered == st.tables[1].classes);
    CHECK(st.tables[1].structures > 0);
    CHECK(db.num_cells() == st.tables[0].covered + st.tables[1].covered);
    CHECK(db.num_inputs() == 3);

    std::ostringstream out{};
    st.report(out);
    CHECK(out.str().find("3 inputs") != std::string::npos);

    check_all_functions(db, 2);
    check_all_functions(db, 3);

    // the cell of a function with a smaller support comes from the smaller table
    kitty::dynamic_truth_table a{3};
    kitty::dynamic_truth_table b{3};
    kitty::create_nth_var(a, 0);
    kitty::create_nth_var(b, 1);
    const auto inst = db.lookup(a & ~b);
    REQUIRE(inst.has_value());
    CHECK(inst->cell.num_inputs == 2);
    CHECK(inst->support == std::vector<uint8_t>{0, 1});
    CHECK(inst->cell.num_gates() == 1);
    CHECK(inst->cell.num_inversions() <= 2);

    // constants and single variables get a cell without nodes
    const auto one = db.lookup(~kitty::dynamic_truth_table{3});
    REQUIRE(one.has_value());
    CHECK(one->cell.num_gates() == 0);
    CHECK(one->cell.output_inverted);
    CHECK(one->support.empty());
    const auto not_b = db.lookup(~b);
    REQUIRE(not_b.has_value());
    CHECK(not_b->cell.num_gates() == 0);
    CHECK(not_b->cell.output_inverted);
    CHECK(not_b->support == std::vector<uint8_t>{1});

    // every stored cell computes its representative and uses all its inputs
    db.foreach_cell(3,
                    [](const uint64_t rep, const planar_cell& cell)
                    {
                        CHECK(cell.function() == rep);
                        CHECK(cell.num_inputs == 3);
                        CHECK(fiction::synthesis::detail::tt_word::has_full_support(rep, 3));
                    });
}

TEST_CASE("The gate set bounds the cells", "[planar-cell-database]")
{
    // without MAJ no node has three fanins, and the three-input functions are still all covered
    planar_cell_database_params ps{};
    ps.num_inputs = 3;
    ps.gate_set   = {planar_gate::AND, planar_gate::XOR};

    planar_cell_database_stats st{};
    const auto                 db = generate_planar_cells(ps, &st);
    CHECK(st.tables[1].covered == st.tables[1].classes);
    db.foreach_cell(3,
                    [](const uint64_t, const planar_cell& cell)
                    {
                        for (const auto& n : cell.nodes)
                        {
                            CHECK(n.fanins.size() == 2);
                            CHECK(n.gate != planar_gate::MAJ);
                            CHECK(n.gate != planar_gate::OR);
                        }
                    });
    check_all_functions(db, 3);

    // without XOR the xor-type class of two inputs has no planar cell
    planar_cell_database_params or_only{};
    or_only.num_inputs = 2;
    or_only.gate_set   = {planar_gate::OR};

    planar_cell_database_stats st2{};
    const auto                 db2 = generate_planar_cells(or_only, &st2);
    CHECK(st2.tables[0].classes == 2);
    CHECK(st2.tables[0].covered == 1);
    kitty::dynamic_truth_table x{2};
    kitty::create_from_binary_string(x, "0110");
    CHECK(!db2.lookup(x).has_value());

    // the members of the class are served with the cheapest negations: OR itself costs one gate, NOR one more
    kitty::create_from_binary_string(x, "1110");
    const auto inst = db2.lookup(x);
    REQUIRE(inst.has_value());
    CHECK(inst->cell.cost() == 1);
    CHECK(inst->cell.nodes.front().gate == planar_gate::OR);
    kitty::create_from_binary_string(x, "0001");
    const auto nor = db2.lookup(x);
    REQUIRE(nor.has_value());
    CHECK(nor->cell.cost() == 2);
    CHECK(nor->cell.output_inverted);
    kitty::create_from_binary_string(x, "1000");
    const auto conj = db2.lookup(x);
    REQUIRE(conj.has_value());
    CHECK(conj->cell.cost() == 4);  // and(a, b) = ~or(~a, ~b)
    CHECK(expand(*conj, 2) == x);
}

TEST_CASE("The level bound limits the structures", "[planar-cell-database]")
{
    planar_cell_database_params one_level{};
    one_level.num_inputs = 3;
    one_level.max_levels = 1;

    planar_cell_database_stats st{};
    const auto                 db = generate_planar_cells(one_level, &st);
    CHECK(st.tables[1].structures == 1);  // maj(0, 1, 2)
    CHECK(st.tables[1].covered < st.tables[1].classes);
    db.foreach_cell(3, [](const uint64_t, const planar_cell& cell) { CHECK(cell.depth() == 1); });

    planar_cell_database_params two_levels{};
    two_levels.num_inputs = 3;
    two_levels.max_levels = 2;
    planar_cell_database_stats st2{};
    const auto                 db2 = generate_planar_cells(two_levels, &st2);
    CHECK(st2.tables[1].structures > st.tables[1].structures);
    CHECK(st2.tables[1].covered >= st.tables[1].covered);
}

TEST_CASE("Four-input cells at three levels", "[planar-cell-database]")
{
    // the 2024 enumeration's result: 1070 of the 2191 negation classes of functions that depend on all four inputs
    planar_cell_database_params ps{};
    ps.num_inputs = 4;
    ps.max_levels = 3;
    ps.gate_set   = {planar_gate::AND, planar_gate::XOR, planar_gate::MAJ};

    planar_cell_database_stats st{};
    const auto                 db = generate_planar_cells(ps, &st);
    REQUIRE(st.tables.size() == 3);
    CHECK(st.tables[2].num_inputs == 4);
    CHECK(st.tables[2].structures == 208);
    CHECK(st.tables[2].classes == 2191);
    CHECK(st.tables[2].covered == 1070);
    CHECK(db.num_cells(4) == 1070);

    // every stored cell computes its representative with at most three levels and at most two readers per signal
    db.foreach_cell(4,
                    [](const uint64_t rep, const planar_cell& cell)
                    {
                        CHECK(cell.function() == rep);
                        CHECK(cell.depth() <= 3);
                        std::vector<uint32_t> readers(4 + cell.nodes.size(), 0);
                        for (const auto& n : cell.nodes)
                        {
                            for (const auto f : n.fanins)
                            {
                                ++readers[f];
                            }
                        }
                        CHECK(std::all_of(readers.begin(), readers.end(), [](const auto r) { return r <= 2; }));
                    });

    // a function with a cell instantiates, one without reports it
    kitty::dynamic_truth_table xor4{4};
    kitty::create_from_hex_string(xor4, "6996");
    const auto parity = db.lookup(xor4);
    REQUIRE(parity.has_value());
    CHECK(expand(*parity, 4) == xor4);
    CHECK(parity->cell.num_gates() == 3);
    CHECK(parity->cell.num_inversions() == 0);

    kitty::dynamic_truth_table uncovered{4};
    kitty::create_from_hex_string(uncovered, "1668");
    CHECK(!db.lookup(uncovered).has_value());
}

TEST_CASE("Extending a database fills only its gaps", "[planar-cell-database]")
{
    // one level: only the majority cell of three inputs
    planar_cell_database_params one_level{};
    one_level.num_inputs = 3;
    one_level.max_levels = 1;
    auto db              = generate_planar_cells(one_level);
    REQUIRE(db.num_cells(3) == 1);
    const auto  before = db.num_cells();
    uint64_t    maj_rep{0};
    std::string maj_string{};
    db.foreach_cell(3,
                    [&maj_rep, &maj_string](const uint64_t rep, const planar_cell& cell)
                    {
                        maj_rep    = rep;
                        maj_string = cell.to_string();
                    });

    // filling with three levels adds the other classes and keeps the majority cell although a cheaper cell exists
    planar_cell_database_params three_levels{};
    three_levels.num_inputs = 3;
    three_levels.max_levels = 3;
    planar_cell_database_stats st{};
    const auto                 added = extend_planar_cells(db, three_levels, &st);
    CHECK(added == db.num_cells() - before);
    CHECK(st.tables[1].covered == st.tables[1].classes);
    CHECK(db.num_cells(3) == st.tables[1].classes);
    REQUIRE(db.cell_of(maj_rep, 3) != nullptr);
    CHECK(db.cell_of(maj_rep, 3)->to_string() == maj_string);
    check_all_functions(db, 3);

    // a database that is complete already is left alone, and the run stops at once
    planar_cell_database_stats again{};
    CHECK(extend_planar_cells(db, three_levels, &again) == 0);
    CHECK(again.tables[1].evaluations == 0);
}

TEST_CASE("A node function folds its negations", "[planar-cell-database]")
{
    kitty::dynamic_truth_table expected{2};

    kitty::create_from_binary_string(expected, "0111");  // nand
    CHECK(detail::planar_cell_node_function(planar_gate::AND, 2, 0b00, true) == expected);
    kitty::create_from_binary_string(expected, "0100");  // ~a & b = a < b
    CHECK(detail::planar_cell_node_function(planar_gate::AND, 2, 0b01, false) == expected);
    kitty::create_from_binary_string(expected, "0010");  // a & ~b = a > b
    CHECK(detail::planar_cell_node_function(planar_gate::AND, 2, 0b10, false) == expected);
    kitty::create_from_binary_string(expected, "0001");  // nor
    CHECK(detail::planar_cell_node_function(planar_gate::AND, 2, 0b11, false) == expected);
    kitty::create_from_binary_string(expected, "1101");  // ~(a & ~b) = a <= b
    CHECK(detail::planar_cell_node_function(planar_gate::AND, 2, 0b10, true) == expected);
    kitty::create_from_binary_string(expected, "1011");  // a | ~b = a >= b
    CHECK(detail::planar_cell_node_function(planar_gate::OR, 2, 0b10, false) == expected);
    kitty::create_from_binary_string(expected, "1001");  // xnor
    CHECK(detail::planar_cell_node_function(planar_gate::XOR, 2, 0b01, false) == expected);
    CHECK(detail::planar_cell_node_function(planar_gate::XOR, 2, 0b00, true) == expected);
    kitty::create_from_binary_string(expected, "0110");  // two negations cancel in an xor
    CHECK(detail::planar_cell_node_function(planar_gate::XOR, 2, 0b11, false) == expected);

    kitty::dynamic_truth_table a{3};
    kitty::dynamic_truth_table b{3};
    kitty::dynamic_truth_table c{3};
    kitty::create_nth_var(a, 0);
    kitty::create_nth_var(b, 1);
    kitty::create_nth_var(c, 2);
    const auto maj = ~((~a & b) | (~a & c) | (b & c));
    CHECK(detail::planar_cell_node_function(planar_gate::MAJ, 3, 0b001, true) == maj);
}

TEST_CASE("Free inversions select cells by gates alone", "[planar-cell-database]")
{
    planar_cell_database_params ps{};
    ps.num_inputs = 3;
    ps.max_levels = 3;
    ps.gate_set   = {planar_gate::AND, planar_gate::OR, planar_gate::XOR};

    planar_cell_database_stats counted_st{};
    const auto                 counted = generate_planar_cells(ps, &counted_st);
    ps.free_inversions                 = true;
    planar_cell_database_stats free_st{};
    const auto                 free = generate_planar_cells(ps, &free_st);

    CHECK(!counted.free_inversions());
    CHECK(free.free_inversions());

    // the same classes are covered, and every function is still served
    CHECK(free_st.tables[1].covered == counted_st.tables[1].covered);
    CHECK(free.num_cells() == counted.num_cells());
    check_all_functions(free, 3);

    // per class the free cell has the same depth and at most as many gates, and its class cost counts gates alone
    uint64_t gates_free    = 0;
    uint64_t gates_counted = 0;
    for (uint32_t k = 2; k <= 3; ++k)
    {
        free.foreach_cell(k,
                          [&counted, &free, &gates_free, &gates_counted, k](const uint64_t rep, const planar_cell& cell)
                          {
                              const auto* other = counted.cell_of(rep, k);
                              REQUIRE(other != nullptr);
                              CHECK(cell.depth() == other->depth());
                              CHECK(cell.num_gates() <= other->num_gates());
                              const auto* e = free.entry_of(rep, k);
                              REQUIRE(e != nullptr);
                              CHECK(e->class_cost == static_cast<uint64_t>(e->members) * cell.num_gates());
                              gates_free += cell.num_gates();
                              gates_counted += other->num_gates();
                          });
    }
    CHECK(gates_free <= gates_counted);

    // the flag of an extended database is its own
    auto extended = generate_planar_cells(ps);
    ps.max_levels = 4;
    extend_planar_cells(extended, ps);
    CHECK(extended.free_inversions());
}
