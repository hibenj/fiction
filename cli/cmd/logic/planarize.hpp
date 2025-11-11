//
// Created by benjamin on 28.10.2025.
//

#ifndef FICTION_PLANARIZE_HPP
#define FICTION_PLANARIZE_HPP

#include <fiction/algorithms/network_transformation/node_duplication_planarization.hpp>
#include <fiction/algorithms/network_transformation/network_balancing.hpp>
#include <fiction/algorithms/network_transformation/fanout_substitution.hpp>
#include <fiction/networks/views/mutable_rank_view.hpp>

#include <alice/alice.hpp>
#include <iostream>

namespace alice
{

/**
 * Performs planarization on the current logic network in store.
 *
 * This command applies fanout substitution and path balancing if necessary,
 * then executes the node duplication planarization algorithm on the resulting
 * network to eliminate crossings.
 */
class planarize_command : public command
{
  public:
    explicit planarize_command(const environment::ptr& e) :
            command(e,
                    "Performs planarization on the current logic network using node duplication. "
                    "If the network is not yet fanout-substituted or balanced, those steps are "
                    "automatically applied before planarization.")
    {
        add_flag("--random_po_order,-r", random_po_order,
                 "Randomize primary output order during node duplication planarization");
    }

  protected:
    void execute() override
    {
        auto& s = store<fiction::logic_network_t>();

        // --- Step 1: Check for empty store ---
        if (s.empty())
        {
            env->out() << "[w] no logic network in store" << std::endl;
            return;
        }

        // --- Step 2: Prepare planarization parameters ---
        fiction::node_duplication_planarization_params planar_params{};
        planar_params.po_order = random_po_order
                                     ? fiction::node_duplication_planarization_params::output_order::RANDOM_PO_ORDER
                                     : fiction::node_duplication_planarization_params::output_order::KEEP_PO_ORDER;

        // --- Step 3: Main procedure ---
        const auto process_network = [&](auto&& ntk_ptr) -> std::shared_ptr<fiction::technology_network> {
            auto tec = fiction::convert_network<fiction::tec_nt>(*ntk_ptr);

            // --- Fanout substitution ---
            if (!fiction::is_fanout_substituted(tec))
            {
                env->out() << "[i] performing fanout substitution..." << '\n';
                tec = fiction::fanout_substitution<fiction::tec_nt>(tec);
            }
            else
            {
                env->out() << "[i] network already fanout-substituted" << '\n';
            }

            // --- Network balancing ---
            fiction::network_balancing_params bal_params;
            bal_params.unify_outputs = true;

            if (!fiction::is_balanced(*ntk_ptr))
            {
                env->out() << "[i] performing path balancing..." << '\n';
                tec = fiction::network_balancing<fiction::technology_network>(tec, bal_params);
            }
            else
            {
                env->out() << "[i] network already balanced" << '\n';
            }

            // --- Planarization ---
            env->out() << "[i] performing node duplication planarization..." << '\n';
            auto rank_view     = fiction::mutable_rank_view(tec);
            auto planarized_ntk = fiction::node_duplication_planarization(rank_view, planar_params);

            env->out() << "[i] planarization complete" << '\n';
            return std::make_shared<fiction::technology_network>(planarized_ntk);
        };

        // --- Step 4: Apply to store ---
        s.extend() = std::visit(process_network, s.current());
    }

  private:
    bool random_po_order = false;
};

ALICE_ADD_COMMAND(planarize, "Logic")

}  // namespace alice

#endif  // FICTION_PLANARIZE_HPP
