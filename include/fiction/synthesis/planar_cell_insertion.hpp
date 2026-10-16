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
 * @brief Inserts the planar cell of a concrete function into a logic network.
 * @author Benjamin Hien (hibenj)
 */

#pragma once

#include "fiction/synthesis/planar_cell_database.hpp"

#include <kitty/dynamic_truth_table.hpp>
#include <mockturtle/traits.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace fiction::synthesis
{

/**
 * Parameters of the planar cell insertion.
 */
struct planar_cell_insertion_params
{
    /**
     * Whether an inverted edge is folded into the gate that reads it instead of becoming a `create_not`. The gate is
     * then created with the function that includes its negations (`detail::planar_cell_node_function`), which in a
     * `technology_network` is one of NAND, NOR, XNOR, LT, GT, LE and GE next to AND, OR and XOR: the gates of the
     * Bestagon skeleton library, which reads inverted signals in the gates themselves. The output inversion of a
     * cell is folded into its last gate the same way, and a majority with inverted fanins becomes a three-input
     * node of that function. Only a cell without gates, the NOT of one variable, still creates an inverter. Needs
     * `create_node` on the network; a network that carries complements on its signals ignores the flag, because its
     * inversions are free anyway.
     */
    bool absorb_inversions = false;
};

/**
 * The nodes a planar cell contributed to a network.
 *
 * @tparam Ntk Network type.
 */
template <typename Ntk>
struct planar_cell_instance
{
    /**
     * The signal that computes the function.
     */
    mockturtle::signal<Ntk> output{};
    /**
     * The gate nodes per cell level, level 1 first, in the left-to-right order of the cell. Inverters sit on the
     * level of the gate that reads them and are listed in `inverters` instead.
     */
    std::vector<std::vector<mockturtle::node<Ntk>>> gates{};
    /**
     * The inverter nodes created for inverted edges, in creation order. Empty for networks whose signals carry the
     * complement themselves.
     */
    std::vector<mockturtle::node<Ntk>> inverters{};
    /**
     * The cell that was inserted, negations folded in; its nodes without a network counterpart for a constant or a
     * single variable.
     */
    planar_cell cell{};
};

/**
 * Creates the planar cell of a function in a network: the cheapest cell of the function's negation class with the
 * negations realized on its edges, built from the given fanin signals in the function's variable order. A function
 * that does not depend on all variables uses the cell of its support; a constant yields the constant signal and a
 * single variable the fanin itself, inverted if needed. Every inverted edge becomes a `create_not`, which is an
 * inverter node in a `technology_network` and a complemented signal in networks that carry complements on their
 * signals, or, with `ps.absorb_inversions`, is folded into the function of the gate that reads it. The fragment is
 * planar in a network that has every gate of the cell natively; a network that builds XOR or majority from other
 * gates loses the property.
 *
 * @tparam Ntk Network type with `create_and`, `create_or`, `create_xor`, `create_maj` and `create_not`, and
 * `create_node` to absorb inversions.
 * @param ntk The network to insert into.
 * @param db The cell database.
 * @param f The function, over at most `db.num_inputs()` effective variables.
 * @param fanins The signal of every variable of `f`, `f.num_vars()` of them in variable order.
 * @param ps Parameters.
 * @return The inserted cell, or `std::nullopt` if the database has no cell for the function's class.
 */
template <typename Ntk>
[[nodiscard]] std::optional<planar_cell_instance<Ntk>>
insert_planar_cell(Ntk& ntk, const planar_cell_database& db, const kitty::dynamic_truth_table& f,
                   const std::vector<mockturtle::signal<Ntk>>& fanins, const planar_cell_insertion_params& ps = {})
{
    static_assert(mockturtle::is_network_type_v<Ntk>, "Ntk is not a network type");
    static_assert(mockturtle::has_create_and_v<Ntk>, "Ntk does not implement the create_and function");
    static_assert(mockturtle::has_create_or_v<Ntk>, "Ntk does not implement the create_or function");
    static_assert(mockturtle::has_create_xor_v<Ntk>, "Ntk does not implement the create_xor function");
    static_assert(mockturtle::has_create_maj_v<Ntk>, "Ntk does not implement the create_maj function");
    static_assert(mockturtle::has_create_not_v<Ntk>, "Ntk does not implement the create_not function");
    static_assert(mockturtle::has_get_constant_v<Ntk>, "Ntk does not implement the get_constant function");
    static_assert(mockturtle::has_get_node_v<Ntk>, "Ntk does not implement the get_node function");
    static_assert(mockturtle::has_is_constant_v<Ntk>, "Ntk does not implement the is_constant function");

    assert(fanins.size() == f.num_vars());

    const auto looked_up = db.lookup(f);
    if (!looked_up.has_value())
    {
        return std::nullopt;
    }

    planar_cell_instance<Ntk> inst{};
    inst.cell = looked_up->cell;

    // the signals of the cell: its inputs in support order, then its nodes
    std::vector<mockturtle::signal<Ntk>> signals{};
    signals.reserve(looked_up->support.size() + inst.cell.nodes.size());
    for (const auto v : looked_up->support)
    {
        signals.push_back(fanins.at(v));
    }

    // a constant or a single variable has no gate
    if (inst.cell.nodes.empty())
    {
        if (signals.empty())
        {
            inst.output = ntk.get_constant(inst.cell.output_inverted);
            return inst;
        }
        inst.output = inst.cell.output_inverted ? ntk.create_not(signals.front()) : signals.front();
        if (inst.cell.output_inverted && !ntk.is_constant(ntk.get_node(inst.output)) &&
            ntk.get_node(inst.output) != ntk.get_node(signals.front()))
        {
            inst.inverters.push_back(ntk.get_node(inst.output));
        }
        return inst;
    }

    // whether the negations are folded into the gates: only a network with arbitrary node functions can do that; a
    // network that carries complements on its signals has free inversions anyway
    constexpr bool can_absorb = mockturtle::has_create_node_v<Ntk>;
    const bool     absorb     = ps.absorb_inversions && can_absorb;

    const auto read = [&ntk, &inst, &signals, absorb](const planar_cell_node& n, const std::size_t i)
    {
        const auto s = signals[n.fanins[i]];
        if (absorb || ((static_cast<uint32_t>(n.inverted_fanins) >> i) & 1u) == 0u)
        {
            return s;
        }
        const auto inv = ntk.create_not(s);
        if (ntk.get_node(inv) != ntk.get_node(s))
        {
            inst.inverters.push_back(ntk.get_node(inv));
        }
        return inv;
    };

    inst.gates.resize(inst.cell.depth());
    for (std::size_t j = 0; j < inst.cell.nodes.size(); ++j)
    {
        const auto&             n = inst.cell.nodes[j];
        mockturtle::signal<Ntk> s{};
        if (absorb)
        {
            if constexpr (can_absorb)
            {
                std::vector<mockturtle::signal<Ntk>> gate_fanins{};
                gate_fanins.reserve(n.fanins.size());
                for (std::size_t i = 0; i < n.fanins.size(); ++i)
                {
                    gate_fanins.push_back(read(n, i));
                }
                const bool last = j + 1 == inst.cell.nodes.size();
                s = ntk.create_node(gate_fanins, detail::planar_cell_node_function(
                                                     n.gate, static_cast<uint32_t>(n.fanins.size()), n.inverted_fanins,
                                                     last && inst.cell.output_inverted));
            }
        }
        else
        {
            switch (n.gate)
            {
                case planar_gate::AND:
                {
                    s = ntk.create_and(read(n, 0), read(n, 1));
                    break;
                }
                case planar_gate::OR:
                {
                    s = ntk.create_or(read(n, 0), read(n, 1));
                    break;
                }
                case planar_gate::XOR:
                {
                    s = ntk.create_xor(read(n, 0), read(n, 1));
                    break;
                }
                case planar_gate::MAJ:
                {
                    s = ntk.create_maj(read(n, 0), read(n, 1), read(n, 2));
                    break;
                }
            }
        }
        signals.push_back(s);
        inst.gates[n.level - 1].push_back(ntk.get_node(s));
    }

    inst.output = signals.back();
    if (inst.cell.output_inverted && !absorb)
    {
        inst.output = ntk.create_not(inst.output);
        if (ntk.get_node(inst.output) != ntk.get_node(signals.back()))
        {
            inst.inverters.push_back(ntk.get_node(inst.output));
        }
    }

    return inst;
}

/**
 * Builds a network that consists of one planar cell: one primary input per variable of `f` (unused variables stay
 * dangling inputs) and one primary output computing `f`.
 *
 * @tparam Ntk Network type, see `insert_planar_cell`.
 * @param db The cell database.
 * @param f The function.
 * @param ps Parameters of the insertion.
 * @return The network, or `std::nullopt` if the database has no cell for the function's class.
 */
template <typename Ntk>
[[nodiscard]] std::optional<Ntk> planar_cell_network(const planar_cell_database&         db,
                                                     const kitty::dynamic_truth_table&   f,
                                                     const planar_cell_insertion_params& ps = {})
{
    static_assert(mockturtle::has_create_pi_v<Ntk>, "Ntk does not implement the create_pi function");
    static_assert(mockturtle::has_create_po_v<Ntk>, "Ntk does not implement the create_po function");

    Ntk                                  ntk{};
    std::vector<mockturtle::signal<Ntk>> pis{};
    pis.reserve(f.num_vars());
    for (uint32_t i = 0; i < f.num_vars(); ++i)
    {
        pis.push_back(ntk.create_pi());
    }

    const auto inst = insert_planar_cell(ntk, db, f, pis, ps);
    if (!inst.has_value())
    {
        return std::nullopt;
    }
    ntk.create_po(inst->output);

    return ntk;
}

}  // namespace fiction::synthesis
