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
 * @brief Tests for `fiction/synthesis/io/planar_cell_database_json.hpp`.
 * @author Benjamin Hien (hibenj)
 */

#include <catch2/catch_test_macros.hpp>

#include <fiction/synthesis/io/planar_cell_database_json.hpp>
#include <fiction/synthesis/planar_cell_database.hpp>

#include <cstdint>
#include <sstream>
#include <string>

using namespace fiction::synthesis;
using namespace fiction::synthesis::io;

TEST_CASE("Planar cell databases survive a round trip through JSON", "[planar-cell-database-json]")
{
    planar_cell_database_params ps{};
    ps.num_inputs = 3;
    ps.gate_set   = {planar_gate::AND, planar_gate::OR, planar_gate::XOR, planar_gate::MAJ};

    const auto db = generate_planar_cells(ps);

    std::stringstream stream{};
    write_planar_cell_database(db, stream, &ps);

    const auto text = stream.str();
    CHECK(text.find("\"format\": \"fiction planar cell database\"") != std::string::npos);
    CHECK(text.find("\"max_levels\": 3") != std::string::npos);
    CHECK(text.find("\"gate_set\"") != std::string::npos);

    const auto copy = read_planar_cell_database(stream);
    CHECK(copy.num_inputs() == db.num_inputs());
    CHECK(copy.num_cells(2) == db.num_cells(2));
    CHECK(copy.num_cells(3) == db.num_cells(3));
    for (uint32_t k = 2; k <= 3; ++k)
    {
        db.foreach_cell(k,
                        [&copy, k](const uint64_t rep, const planar_cell& cell)
                        {
                            const auto* c = copy.cell_of(rep, k);
                            REQUIRE(c != nullptr);
                            CHECK(c->function() == rep);
                            CHECK(c->to_string() == cell.to_string());
                            CHECK(c->cost() == cell.cost());
                        });
    }

    // without parameters no metadata is written
    std::stringstream bare{};
    write_planar_cell_database(db, bare);
    CHECK(bare.str().find("max_levels") == std::string::npos);
    CHECK(read_planar_cell_database(bare).num_cells() == db.num_cells());
}

TEST_CASE("Malformed planar cell databases are rejected", "[planar-cell-database-json]")
{
    std::stringstream not_json{"{"};
    CHECK_THROWS_AS(read_planar_cell_database(not_json), planar_cell_database_parsing_error);

    std::stringstream wrong_format{R"({"format": "something else", "version": 1, "num_inputs": 2, "cells": []})"};
    CHECK_THROWS_AS(read_planar_cell_database(wrong_format), planar_cell_database_parsing_error);

    std::stringstream missing_cells{R"({"format": "fiction planar cell database", "version": 1, "num_inputs": 2})"};
    CHECK_THROWS_AS(read_planar_cell_database(missing_cells), planar_cell_database_parsing_error);

    // a cell that claims a function it does not compute
    std::stringstream wrong_function{R"({"format": "fiction planar cell database", "version": 1, "num_inputs": 2,
        "cells": [{"inputs": 2, "function": "6", "cost": 1, "depth": 1,
                   "nodes": [{"gate": "and", "fanins": [0, 1], "inverted": [false, false], "level": 1}],
                   "output_inverted": false}]})"};
    CHECK_THROWS_AS(read_planar_cell_database(wrong_function), planar_cell_database_parsing_error);

    // an unknown gate
    std::stringstream unknown_gate{R"({"format": "fiction planar cell database", "version": 1, "num_inputs": 2,
        "cells": [{"inputs": 2, "function": "8", "cost": 1, "depth": 1,
                   "nodes": [{"gate": "nand", "fanins": [0, 1], "inverted": [false, false], "level": 1}],
                   "output_inverted": false}]})"};
    CHECK_THROWS_AS(read_planar_cell_database(unknown_gate), planar_cell_database_parsing_error);

    // a correct one-cell database
    std::stringstream good{R"({"format": "fiction planar cell database", "version": 1, "num_inputs": 2,
        "cells": [{"inputs": 2, "function": "1", "cost": 2, "depth": 1,
                   "nodes": [{"gate": "or", "fanins": [0, 1], "inverted": [false, false], "level": 1}],
                   "output_inverted": true}]})"};
    const auto        db = read_planar_cell_database(good);
    CHECK(db.num_cells() == 1);
    REQUIRE(db.cell_of(0x1u, 2) != nullptr);
    CHECK(db.cell_of(0x1u, 2)->to_string() == "or(0,1)@1 ~");
}
