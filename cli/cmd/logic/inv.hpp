//
// Created by benjamin on 20.04.23.
//

#ifndef FICTION_INV_HPP
#define FICTION_INV_HPP

#include <fiction/algorithms/network_transformation/inverter_substitution.hpp>
#include <fiction/traits.hpp>
#include <fiction/types.hpp>

#include <alice/alice.hpp>
#include <fmt/format.h>

#include <memory>
#include <string>
#include <variant>

namespace alice
{
/**
 * Creates a new logic network of the current logic network type in store and performs a inverter substitution
 */
class inv_command : public command
{
  public:
    /**
     * Standard constructor. Adds descriptive information, options, and flags.
     *
     * @param e alice::environment that specifies stores etc.
     */
    explicit inv_command(const environment::ptr& e) :
            command(e, "Substitutes two inverters at the fan-outs of fan-out nodes with one inverter at their fan-in.")
    {}

  protected:
    void execute() override
    {
        auto& s = store<fiction::logic_network_t>();
        if (s.empty())
        {
            env->out() << "[w] no logic network in store" << std::endl;

            return;
        }

        const auto substitute_inverters = [this](auto&& ntk_ptr)
        {
            using Ntk = typename std::decay_t<decltype(ntk_ptr)>::element_type;
            if constexpr (fiction::has_is_fanout_v<Ntk>)
            {
                auto optimized_ntk{fiction::inverter_substitution(*ntk_ptr)};
                auto gate_dif = (*ntk_ptr).num_gates() - optimized_ntk.num_gates();
                env->out() << "[i] Number of reduced inverters: " << gate_dif << std::endl;

                result = optimized_ntk;
            }
            else
            {
                env->out() << "[e] network is not fan-out substituted. You may want to try command 'fanouts' before "
                              "running this command."
                           << std::endl;
            }
        };

        try
        {
            std::visit(substitute_inverters, s.current());
            s.extend() = std::make_shared<fiction::tec_nt>(result);
        }
        catch (...)
        {
            env->out() << "[e] an error occurred while reading the technology network" << std::endl;
        }
        result = {};
    }

  private:
    mockturtle::names_view<fiction::technology_network> result{};
};

ALICE_ADD_COMMAND(inv, "Logic")

}  // namespace alice

#endif  // FICTION_INV_HPP