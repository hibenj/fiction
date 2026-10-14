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
 * @brief Generates planar cell databases for several gate sets and reports their coverage.
 *
 * For every gate set, the enumeration runs for all support sizes up to `--k` with `--levels` gate levels, writes
 * the database as JSON to `--out`, and reports per support size the structures, evaluations, negation classes and
 * covered classes, plus, for the largest support size, how many NPN classes have a cell for every, for some or for
 * no input order and the cost range of the cells. `--gate-set <g,g,..>` may be given several times; without it the
 * sets {AND, XOR}, {AND, OR, XOR} and {AND, OR, XOR, MAJ} are run. With `--base <file>`, the enumeration only fills
 * the gaps of that database (its cells are kept, a cell is added for every class without one, and the run stops
 * once all classes are covered), which adds the few deep cells of a larger level bound to a shallow database.
 */

#include "fiction_experiments.hpp"

#include <fiction/synthesis/io/planar_cell_database_json.hpp>
#include <fiction/synthesis/planar_cell_database.hpp>

#include <fmt/format.h>
#include <kitty/constructors.hpp>
#include <kitty/dynamic_truth_table.hpp>
#include <kitty/npn.hpp>
#include <kitty/operations.hpp>
#include <kitty/print.hpp>
#include <mockturtle/utils/stopwatch.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

using namespace fiction::synthesis;

namespace
{

constexpr std::string_view K_OPTION        = "--k";
constexpr std::string_view LEVELS_OPTION   = "--levels";
constexpr std::string_view GATE_SET_OPTION = "--gate-set";
constexpr std::string_view OUT_OPTION      = "--out";
constexpr std::string_view BASE_OPTION     = "--base";

/**
 * Parses a comma-separated gate list such as `and,or,xor,maj`.
 */
std::vector<planar_gate> parse_gate_set(const std::string& list)
{
    std::vector<planar_gate> gates{};
    std::size_t              pos = 0;
    while (pos <= list.size())
    {
        auto next = list.find(',', pos);
        if (next == std::string::npos)
        {
            next = list.size();
        }
        gates.push_back(io::detail::gate_of(list.substr(pos, next - pos)));
        pos = next + 1;
    }
    return gates;
}

std::string gate_set_name(const std::vector<planar_gate>& gates)
{
    std::string name{};
    for (const auto g : gates)
    {
        if (!name.empty())
        {
            name += '-';
        }
        name += io::detail::gate_name(g);
    }
    return name;
}

/**
 * NPN coverage of the largest support size: classes with a cell for every, for some, for no input order.
 */
struct npn_coverage
{
    uint32_t classes{0};
    uint32_t every_order{0};
    uint32_t some_orders{0};
    uint32_t no_order{0};
    uint32_t min_cost{0};
    uint32_t max_cost{0};
    double   mean_cost{0.0};
};

npn_coverage measure(const planar_cell_database& db, const uint32_t k)
{
    npn_coverage cov{};

    // per NPN class: negation classes with and without a cell
    std::unordered_map<uint64_t, std::pair<uint32_t, uint32_t>> classes{};
    std::unordered_map<uint64_t, bool>                          seen{};

    kitty::dynamic_truth_table f{k};
    do  // NOLINT(cppcoreguidelines-avoid-do-while): kitty enumerates functions until the table wraps to zero
    {
        const auto word = *f.begin() & detail::tt_word::full(k);
        if (detail::tt_word::has_full_support(word, k))
        {
            const auto cls = detail::canonize(word, k);
            if (!seen[cls.representative])
            {
                seen[cls.representative] = true;
                const auto npn           = *std::get<0>(kitty::exact_npn_canonization(f)).begin();
                auto& [covered, total]   = classes[npn];
                ++total;
                if (db.cell_of(cls.representative, k) != nullptr)
                {
                    ++covered;
                }
            }
        }
        kitty::next_inplace(f);
    } while (!kitty::is_const0(f));

    cov.classes = static_cast<uint32_t>(classes.size());
    for (const auto& [npn, counts] : classes)
    {
        if (counts.first == counts.second)
        {
            ++cov.every_order;
        }
        else if (counts.first == 0)
        {
            ++cov.no_order;
        }
        else
        {
            ++cov.some_orders;
        }
    }

    uint64_t sum = 0;
    uint32_t n   = 0;
    db.foreach_cell(k,
                    [&cov, &sum, &n](const uint64_t, const planar_cell& cell)
                    {
                        const auto c = cell.cost();
                        cov.min_cost = n == 0 ? c : std::min(cov.min_cost, c);
                        cov.max_cost = std::max(cov.max_cost, c);
                        sum += c;
                        ++n;
                    });
    cov.mean_cost = n == 0 ? 0.0 : static_cast<double>(sum) / n;

    return cov;
}

}  // namespace

int main(const int argc, const char** argv)  // NOLINT
{
    const std::span<const char*> args{argv, static_cast<std::size_t>(argc)};

    uint32_t                              k      = 4;
    uint32_t                              levels = 4;
    std::string                           out    = ".";
    std::string                           base{};
    std::vector<std::vector<planar_gate>> gate_sets{};
    for (std::size_t i = 1; i + 1 < args.size(); ++i)
    {
        const std::string_view arg{args[i]};
        if (arg == K_OPTION)
        {
            k = static_cast<uint32_t>(std::stoul(args[i + 1]));
        }
        else if (arg == LEVELS_OPTION)
        {
            levels = static_cast<uint32_t>(std::stoul(args[i + 1]));
        }
        else if (arg == GATE_SET_OPTION)
        {
            gate_sets.push_back(parse_gate_set(args[i + 1]));
        }
        else if (arg == OUT_OPTION)
        {
            out = args[i + 1];
        }
        else if (arg == BASE_OPTION)
        {
            base = args[i + 1];
        }
    }
    if (gate_sets.empty())
    {
        gate_sets = {{planar_gate::AND, planar_gate::XOR},
                     {planar_gate::AND, planar_gate::OR, planar_gate::XOR},
                     {planar_gate::AND, planar_gate::OR, planar_gate::XOR, planar_gate::MAJ}};
    }

    std::filesystem::create_directories(out);

    experiments::experiment<std::string, uint32_t, uint32_t, uint64_t, uint64_t, uint64_t, uint64_t, uint32_t, uint32_t,
                            uint32_t, uint32_t, uint32_t, uint32_t, double, double, std::string>
        exp{fmt::format("planar_cell_database_k{}_l{}", k, levels),
            "gate set",
            "inputs",
            "levels",
            "structures",
            "evaluations",
            "classes",
            "covered",
            "NPN classes",
            "every order",
            "some orders",
            "no order",
            "min cost",
            "max cost",
            "mean cost",
            "runtime [s]",
            "file"};

    for (const auto& gates : gate_sets)
    {
        planar_cell_database_params ps{};
        ps.num_inputs = k;
        ps.max_levels = levels;
        ps.gate_set   = gates;

        const auto name = gate_set_name(gates);
        fmt::print("[i] gate set {}\n", name);

        planar_cell_database_stats st{};
        const auto                 db = [&base, &ps, &st]()
        {
            if (base.empty())
            {
                return generate_planar_cells(ps, &st);
            }
            auto       filled = io::read_planar_cell_database(base);
            const auto added  = extend_planar_cells(filled, ps, &st);
            fmt::print("[i] {} cells added to {}\n", added, base);
            return filled;
        }();
        st.report();

        const auto file =
            fmt::format("{}/planar_cells_{}_k{}_l{}{}.json", out, name, k, levels, base.empty() ? "" : "_fill");
        io::write_planar_cell_database(db, file, &ps);

        for (const auto& t : st.tables)
        {
            const auto cov = measure(db, t.num_inputs);
            exp(name, t.num_inputs, levels, t.structures, t.evaluations, t.classes, t.covered, cov.classes,
                cov.every_order, cov.some_orders, cov.no_order, cov.min_cost, cov.max_cost, cov.mean_cost,
                t.num_inputs == k ? mockturtle::to_seconds(st.time_total) : 0.0, t.num_inputs == k ? file : "");
        }

        exp.save();
        exp.table();
    }

    return 0;
}
