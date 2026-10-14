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
 * @brief Writes and reads planar cell databases as JSON.
 * @author Benjamin Hien (hibenj)
 */

#pragma once

#include "fiction/synthesis/planar_cell_database.hpp"

#include <fmt/format.h>
#include <kitty/constructors.hpp>
#include <kitty/dynamic_truth_table.hpp>
#include <kitty/print.hpp>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <istream>
#include <ostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace fiction::synthesis::io
{

/**
 * Exception thrown when a planar cell database file cannot be parsed or describes a cell that does not compute
 * the function it claims.
 */
class planar_cell_database_parsing_error : public std::runtime_error
{
  public:
    explicit planar_cell_database_parsing_error(const std::string& msg) : std::runtime_error(msg) {}
};

namespace detail
{

[[nodiscard]] inline std::string_view gate_name(const planar_gate g)
{
    switch (g)
    {
        case planar_gate::AND:
        {
            return "and";
        }
        case planar_gate::OR:
        {
            return "or";
        }
        case planar_gate::XOR:
        {
            return "xor";
        }
        case planar_gate::MAJ:
        {
            return "maj";
        }
    }
    return "and";
}

[[nodiscard]] inline planar_gate gate_of(const std::string_view name)
{
    if (name == "and")
    {
        return planar_gate::AND;
    }
    if (name == "or")
    {
        return planar_gate::OR;
    }
    if (name == "xor")
    {
        return planar_gate::XOR;
    }
    if (name == "maj")
    {
        return planar_gate::MAJ;
    }
    throw planar_cell_database_parsing_error{fmt::format("unknown gate '{}'", name)};
}

}  // namespace detail

/**
 * Writes a planar cell database as JSON. The document holds the format name and version, the largest support size,
 * the enumeration parameters if given (`max_levels`, `gate_set`), and one object per cell with its support size,
 * its function (the class representative as a hexadecimal truth table over the cell's inputs), its cost and depth,
 * its nodes (gate, fanins, which fanins are inverted, level) and whether its output is inverted.
 *
 * @param db The database.
 * @param os Output stream.
 * @param ps The parameters the database was generated with, recorded as metadata if given.
 */
inline void write_planar_cell_database(const planar_cell_database& db, std::ostream& os,
                                       const planar_cell_database_params* ps = nullptr)
{
    nlohmann::ordered_json doc{};
    doc["format"]     = "fiction planar cell database";
    doc["version"]    = 1;
    doc["num_inputs"] = db.num_inputs();
    if (ps != nullptr)
    {
        doc["max_levels"] = ps->max_levels;
        std::vector<std::string> gates{};
        gates.reserve(ps->gate_set.size());
        for (const auto g : ps->gate_set)
        {
            gates.emplace_back(detail::gate_name(g));
        }
        doc["gate_set"] = gates;
    }

    auto& cells = doc["cells"];
    cells       = nlohmann::ordered_json::array();
    for (uint32_t k = 2; k <= db.num_inputs(); ++k)
    {
        // sorted by representative for a stable file
        std::vector<std::pair<uint64_t, const planar_cell*>> table{};
        db.foreach_cell(k, [&table](const uint64_t rep, const planar_cell& cell) { table.emplace_back(rep, &cell); });
        std::sort(table.begin(), table.end(), [](const auto& a, const auto& b) { return a.first < b.first; });

        for (const auto& [rep, cell] : table)
        {
            kitty::dynamic_truth_table tt{k};
            *tt.begin() = rep;

            nlohmann::ordered_json c{};
            c["inputs"]   = k;
            c["function"] = kitty::to_hex(tt);
            c["cost"]     = cell->cost();
            c["depth"]    = cell->depth();
            auto& nodes   = c["nodes"];
            nodes         = nlohmann::ordered_json::array();
            for (const auto& n : cell->nodes)
            {
                nlohmann::ordered_json node{};
                node["gate"]   = detail::gate_name(n.gate);
                node["fanins"] = n.fanins;
                std::vector<bool> inverted{};
                inverted.reserve(n.fanins.size());
                for (std::size_t i = 0; i < n.fanins.size(); ++i)
                {
                    inverted.push_back(((n.inverted_fanins >> i) & 1u) != 0u);
                }
                node["inverted"] = inverted;
                node["level"]    = n.level;
                nodes.push_back(std::move(node));
            }
            c["output_inverted"] = cell->output_inverted;
            cells.push_back(std::move(c));
        }
    }

    os << doc.dump(2) << '\n';
}
/**
 * Writes a planar cell database to a JSON file.
 *
 * @param db The database.
 * @param filename Path of the file to write.
 * @param ps The parameters the database was generated with, recorded as metadata if given.
 */
inline void write_planar_cell_database(const planar_cell_database& db, const std::string& filename,
                                       const planar_cell_database_params* ps = nullptr)
{
    std::ofstream os{};
    os.open(filename, std::ios::out);
    if (!os.is_open())
    {
        throw std::ofstream::failure("could not open file " + filename);
    }
    write_planar_cell_database(db, os, ps);
}
/**
 * Reads a planar cell database written by `write_planar_cell_database`. Every cell is checked to compute the
 * function it claims before it is stored.
 *
 * @param is Input stream.
 * @return The database.
 * @throws planar_cell_database_parsing_error If the document is not a planar cell database or a cell is wrong.
 */
[[nodiscard]] inline planar_cell_database read_planar_cell_database(std::istream& is)
{
    nlohmann::json doc{};
    try
    {
        is >> doc;
    }
    catch (const nlohmann::json::exception& e)
    {
        throw planar_cell_database_parsing_error{std::string{"invalid JSON: "} + e.what()};
    }

    try
    {
        if (doc.at("format").get<std::string>() != "fiction planar cell database")
        {
            throw planar_cell_database_parsing_error{"not a planar cell database"};
        }
        if (doc.at("version").get<int>() != 1)
        {
            throw planar_cell_database_parsing_error{"unsupported version"};
        }

        const auto num_inputs = doc.at("num_inputs").get<uint32_t>();
        if (num_inputs < 2 || num_inputs > 6)
        {
            throw planar_cell_database_parsing_error{"num_inputs must be between 2 and 6"};
        }
        planar_cell_database db{num_inputs};

        for (const auto& c : doc.at("cells"))
        {
            planar_cell cell{};
            const auto  k = c.at("inputs").get<uint32_t>();
            if (k < 2 || k > num_inputs)
            {
                throw planar_cell_database_parsing_error{"cell with an invalid number of inputs"};
            }
            cell.num_inputs = static_cast<uint8_t>(k);
            for (const auto& n : c.at("nodes"))
            {
                planar_cell_node node{};
                node.gate           = detail::gate_of(n.at("gate").get<std::string>());
                node.fanins         = n.at("fanins").get<std::vector<uint8_t>>();
                node.level          = n.at("level").get<uint8_t>();
                const auto inverted = n.at("inverted").get<std::vector<bool>>();
                if (node.fanins.size() != inverted.size() || node.fanins.empty() || node.fanins.size() > 3 ||
                    (node.gate == planar_gate::MAJ) != (node.fanins.size() == 3))
                {
                    throw planar_cell_database_parsing_error{"node with inconsistent fanins"};
                }
                for (std::size_t i = 0; i < inverted.size(); ++i)
                {
                    if (node.fanins[i] >= k + cell.nodes.size())
                    {
                        throw planar_cell_database_parsing_error{"node reads a signal that does not exist yet"};
                    }
                    if (inverted[i])
                    {
                        node.inverted_fanins |= static_cast<uint8_t>(1u << i);
                    }
                }
                cell.nodes.push_back(std::move(node));
            }
            cell.output_inverted = c.at("output_inverted").get<bool>();

            kitty::dynamic_truth_table claimed{k};
            kitty::create_from_hex_string(claimed, c.at("function").get<std::string>());
            if (cell.truth_table() != claimed)
            {
                throw planar_cell_database_parsing_error{"cell " + cell.to_string() + " does not compute " +
                                                         c.at("function").get<std::string>()};
            }
            db.insert(std::move(cell));
        }

        return db;
    }
    catch (const nlohmann::json::exception& e)
    {
        throw planar_cell_database_parsing_error{std::string{"malformed planar cell database: "} + e.what()};
    }
}
/**
 * Reads a planar cell database from a JSON file.
 *
 * @param filename Path of the file to read.
 * @return The database.
 */
[[nodiscard]] inline planar_cell_database read_planar_cell_database(const std::string& filename)
{
    std::ifstream is{};
    is.open(filename, std::ios::in);
    if (!is.is_open())
    {
        throw std::ifstream::failure("could not open file " + filename);
    }
    return read_planar_cell_database(is);
}

}  // namespace fiction::synthesis::io
