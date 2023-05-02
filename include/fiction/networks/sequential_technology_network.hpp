//
// Created by benjamin on 02.05.23.
//

#ifndef FICTION_SEQUENTIAL_TECHNOLOGY_NETWORK_HPP
#define FICTION_SEQUENTIAL_TECHNOLOGY_NETWORK_HPP

#include "mockturtle/networks/sequential.hpp"
#include "technology_network.hpp"

/**
 * Template specialization of mockturtle::sequential for fiction::technology_networks
 * This is part of the Distribution Newtork III: Sequential Circuits
 */

namespace mockturtle
{

template <>
class sequential<fiction::technology_network, false> : public fiction::technology_network
{
  public:
    using Ntk       = fiction::technology_network;
    using base_type = typename Ntk::base_type;
    using storage   = typename Ntk::storage;
    using node      = typename Ntk::node;
    using signal    = typename Ntk::signal;

    struct sequential_information
    {
        uint32_t                num_pis{0};
        uint32_t                num_pos{0};
        std::vector<register_t> registers;
    };

    sequential() : _sequential_storage(std::make_shared<sequential_information>())
    {
        static_assert(std::is_same_v<base_type, klut_network> || std::is_same_v<base_type, cover_network>,
                      "Sequential interfaces extended for unknown network type. Please check the compatibility of implementations.");
    }

    sequential(storage base_storage) :
            Ntk(base_storage),
            _sequential_storage(std::make_shared<sequential_information>())
    {
        static_assert(std::is_same_v<base_type, klut_network> || std::is_same_v<base_type, cover_network>,
                      "Sequential interfaces extended for unknown network type. Please check the compatibility of implementations.");
    }

    uint32_t ri_index(signal const& s) const
    {
        uint32_t i = -1;
        foreach_ri(
            [&](const auto& x, auto index)
            {
                if (x == s)
                {
                    i = index;
                    return false;
                }
                return true;
            });
        return i;
    }

    uint32_t ro_index( node const& n ) const
    {
        return Ntk::pi_index( n ) - _sequential_storage->num_pis;
    }

    node ri_to_ro(signal const& s) const
    {
        return *(this->_storage->inputs.begin() + _sequential_storage->num_pis + ri_index(s));
    }

    signal ro_to_ri( node const& n ) const
    {
        return (*( this->_storage->outputs.begin() + _sequential_storage->num_pos + ro_index(n) - _sequential_storage->num_pis )).data;
    }

    signal create_pi()
    {
        ++_sequential_storage->num_pis;
        return Ntk::create_pi();
    }

    uint32_t create_po(signal const& f)
    {
        ++_sequential_storage->num_pos;
        return Ntk::create_po(f);
    }

    signal create_ro()
    {
        _sequential_storage->registers.emplace_back();
        return Ntk::create_pi();
    }

    uint32_t create_ri(signal const& f)
    {
        return Ntk::create_po(f);
    }

    bool is_combinational() const
    {
        return (static_cast<uint32_t>(this->_storage->inputs.size()) == _sequential_storage->num_pis &&
                static_cast<uint32_t>(this->_storage->outputs.size()) == _sequential_storage->num_pos);
    }

    bool is_ci(node const& n) const
    {
        return std::find(this->_storage->inputs.begin(), this->_storage->inputs.end(), n) !=
               this->_storage->inputs.end();
    }

    bool is_pi(node const& n) const
    {
        const auto end = this->_storage->inputs.begin() + _sequential_storage->num_pis;
        return std::find(this->_storage->inputs.begin(), end, n) != end;
    }

    bool is_ro(node const& n) const
    {
        return std::find(this->_storage->inputs.begin() + _sequential_storage->num_pis, this->_storage->inputs.end(),
                         n) != this->_storage->inputs.end();
    }

    bool is_po(const node& n) const
    {
        const auto end = this->_storage->outputs.begin() + _sequential_storage->num_pos;
        return std::find(this->_storage->outputs.begin(), end, n) != end;
    }

    bool is_ri(node const& n) const
    {
        return std::find(this->_storage->outputs.begin() + _sequential_storage->num_pos, this->_storage->outputs.end(),
                         n) != this->_storage->outputs.end();
    }


    auto num_cis() const
    {
        return static_cast<uint32_t>(this->_storage->inputs.size());
    }

    auto num_cos() const
    {
        return static_cast<uint32_t>(this->_storage->outputs.size());
    }

    auto num_pis() const
    {
        return _sequential_storage->num_pis;
    }

    auto num_pos() const
    {
        return _sequential_storage->num_pos;
    }

    auto num_registers() const
    {
        assert(static_cast<uint32_t>(this->_storage->inputs.size() - _sequential_storage->num_pis) ==
               static_cast<uint32_t>(this->_storage->outputs.size() - _sequential_storage->num_pos));
        return static_cast<uint32_t>(this->_storage->inputs.size() - _sequential_storage->num_pis);
    }

    node pi_at(uint32_t index) const
    {
        assert(index < _sequential_storage->num_pis);
        return *(this->_storage->inputs.begin() + index);
    }

    signal po_at(uint32_t index) const
    {
        assert(index < _sequential_storage->num_pos);
        return (*(this->_storage->outputs.begin() + index)).data;
    }

    node ci_at(uint32_t index) const
    {
        assert(index < this->_storage->inputs.size());
        return *(this->_storage->inputs.begin() + index);
    }

    signal co_at(uint32_t index) const
    {
        assert(index < this->_storage->outputs.size());
        return (*(this->_storage->outputs.begin() + index)).data;
    }

    node ro_at(uint32_t index) const
    {
        assert(index < this->_storage->inputs.size() - _sequential_storage->num_pis);
        return *(this->_storage->inputs.begin() + _sequential_storage->num_pis + index);
    }

    signal ri_at(uint32_t index) const
    {
        assert(index < this->_storage->outputs.size() - _sequential_storage->num_pos);
        return (*(this->_storage->outputs.begin() + _sequential_storage->num_pos + index)).data;
    }

    void set_register(uint32_t index, register_t reg)
    {
        assert(index < _sequential_storage->registers.size());
        _sequential_storage->registers[index] = reg;
    }

    register_t register_at(uint32_t index) const
    {
        assert(index < _sequential_storage->registers.size());
        return _sequential_storage->registers[index];
    }

    template <typename Fn>
    void foreach_ci(Fn&& fn) const
    {
        detail::foreach_element(this->_storage->inputs.begin(), this->_storage->inputs.end(), fn);
    }

    template <typename Fn>
    void foreach_co(Fn&& fn) const
    {
        using IteratorType = decltype(this->_storage->outputs.begin());
        detail::foreach_element_transform<IteratorType, uint32_t>(
            this->_storage->outputs.begin(), this->_storage->outputs.end(), [](auto o) { return o.index; }, fn);
    }

    template <typename Fn>
    void foreach_pi(Fn&& fn) const
    {
        detail::foreach_element(this->_storage->inputs.begin(),
                                this->_storage->inputs.begin() + _sequential_storage->num_pis, fn);
    }

    template <typename Fn>
    void foreach_po(Fn&& fn) const
    {
        using IteratorType = decltype(this->_storage->outputs.begin());
        detail::foreach_element_transform<IteratorType, uint32_t>(
            this->_storage->outputs.begin(), this->_storage->outputs.begin() + _sequential_storage->num_pos,
            [](auto o) { return o.index; }, fn);
    }

    template <typename Fn>
    void foreach_ro(Fn&& fn) const
    {
        detail::foreach_element(this->_storage->inputs.begin() + _sequential_storage->num_pis,
                                this->_storage->inputs.end(), fn);
    }

    template <typename Fn>
    void foreach_ri(Fn&& fn) const
    {
        using IteratorType = decltype(this->_storage->outputs.begin());
        detail::foreach_element_transform<IteratorType, uint32_t>(
            this->_storage->outputs.begin() + _sequential_storage->num_pos, this->_storage->outputs.end(),
            [](auto o) { return o.index; }, fn);
    }

    template <typename Fn>
    void foreach_register(Fn&& fn) const
    {
        static_assert(detail::is_callable_with_index_v<Fn, std::pair<signal, node>, void> ||
                      detail::is_callable_without_index_v<Fn, std::pair<signal, node>, void> ||
                      detail::is_callable_with_index_v<Fn, std::pair<signal, node>, bool> ||
                      detail::is_callable_without_index_v<Fn, std::pair<signal, node>, bool>);

        assert(this->_storage->inputs.size() - _sequential_storage->num_pis ==
               this->_storage->outputs.size() - _sequential_storage->num_pos);
        auto ro = this->_storage->inputs.begin() + _sequential_storage->num_pis;
        auto ri = this->_storage->outputs.begin() + _sequential_storage->num_pos;
        if constexpr (detail::is_callable_without_index_v<Fn, std::pair<signal, node>, bool>)
        {
            while (ro != this->_storage->inputs.end() && ri != this->_storage->outputs.end())
            {
                if (!fn(std::make_pair(ri++, ro++)))
                    return;
            }
        }
        else if constexpr (detail::is_callable_with_index_v<Fn, std::pair<signal, node>, bool>)
        {
            uint32_t index{0};
            while (ro != this->_storage->inputs.end() && ri != this->_storage->outputs.end())
            {
                if (!fn(std::make_pair(ri++, ro++), index++))
                    return;
            }
        }
        else if constexpr (detail::is_callable_without_index_v<Fn, std::pair<signal, node>, void>)
        {
            while (ro != this->_storage->inputs.end() && ri != this->_storage->outputs.end())
            {
                fn(std::make_pair(*ri++, *ro++));
            }
        }
        else if constexpr (detail::is_callable_with_index_v<Fn, std::pair<signal, node>, void>)
        {
            uint32_t index{0};
            while (ro != this->_storage->inputs.end() && ri != this->_storage->outputs.end())
            {
                fn(std::make_pair(*ri++, *ro++), index++);
            }
        }
    }

  public:
    std::shared_ptr<sequential_information> _sequential_storage;
};
}
#endif  // FICTION_SEQUENTIAL_TECHNOLOGY_NETWORK_HPP
