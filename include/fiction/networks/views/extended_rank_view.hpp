//
// Created by benjamin on 6/18/24.
//

#include <fiction/networks/virtual_pi_network.hpp>

#include <mockturtle/networks/detail/foreach.hpp>
#include <mockturtle/traits.hpp>
#include <mockturtle/utils/node_map.hpp>
#include <mockturtle/views/depth_view.hpp>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

#ifndef FICTION_VIRTUAL_RANK_VIEW_HPP
#define FICTION_VIRTUAL_RANK_VIEW_HPP

// this rank view can assign specific ranks to nodes in one level
// to check validity of the rank view you can use check_rank_validity()
template <class Ntk, bool has_rank_interface =
                         mockturtle::has_rank_position_v<Ntk> && mockturtle::has_at_rank_position_v<Ntk> &&
                         mockturtle::has_swap_v<Ntk> && mockturtle::has_width_v<Ntk> &&
                         mockturtle::has_foreach_node_in_rank_v<Ntk> && mockturtle::has_foreach_gate_in_rank_v<Ntk>>
class extended_rank_view
{};

template <class Ntk>
class extended_rank_view<Ntk, true> : public mockturtle::depth_view<Ntk>
{
  public:
    extended_rank_view(Ntk const& ntk) : mockturtle::depth_view<Ntk>(ntk) {}
};

template <class Ntk>
class extended_rank_view<Ntk, false> : public mockturtle::depth_view<Ntk>
{
  public:
    static constexpr bool is_topologically_sorted = true;
    using storage                                 = typename Ntk::storage;
    using node                                    = typename Ntk::node;
    using signal                                  = typename Ntk::signal;

    explicit extended_rank_view() : mockturtle::depth_view<Ntk>(), rank_pos{*this}, ranks{}, max_rank_width{0}
    {
        static_assert(mockturtle::is_network_type_v<Ntk>, "Ntk is not a network type");
        static_assert(mockturtle::has_foreach_node_v<Ntk>, "Ntk does not implement the foreach_node method");
        static_assert(mockturtle::has_get_node_v<Ntk>, "Ntk does not implement the get_node method");
        static_assert(mockturtle::has_num_pis_v<Ntk>, "Ntk does not implement the num_pis method");
        static_assert(mockturtle::has_is_ci_v<Ntk>, "Ntk does not implement the is_ci method");
        static_assert(mockturtle::has_is_constant_v<Ntk>, "Ntk does not implement the is_constant method");

        add_event = Ntk::events().register_add_event([this](auto const& n) { on_add(n); });
    }

    /*! \brief Standard constructor.
     *
     * \param ntk Base network
     */
    explicit extended_rank_view(Ntk const& ntk) :
            mockturtle::depth_view<Ntk>{ntk},
            rank_pos{ntk},
            ranks{this->depth() + 1},
            max_rank_width{0}
    {
        static_assert(mockturtle::is_network_type_v<Ntk>, "Ntk is not a network type");
        static_assert(mockturtle::has_foreach_node_v<Ntk>, "Ntk does not implement the foreach_node method");
        static_assert(mockturtle::has_get_node_v<Ntk>, "Ntk does not implement the get_node method");
        static_assert(mockturtle::has_num_pis_v<Ntk>, "Ntk does not implement the num_pis method");
        static_assert(mockturtle::has_is_ci_v<Ntk>, "Ntk does not implement the is_ci method");
        static_assert(mockturtle::has_is_constant_v<Ntk>, "Ntk does not implement the is_constant method");

        init_ranks();

        add_event = Ntk::events().register_add_event([this](auto const& n) { on_add(n); });
    }
    /*!
     * \brief Overloaded constructor with ranks specification.
     *
     * This constructor is similar to the standard one, with the addition of specifying initial node ranks within the
     * network.
     *
     * \param ntk Base network
     * \param ranks A vector of vectors containing nodes, each sub-vector represents a particular rank, and the nodes
     * within it.
     */
    explicit extended_rank_view(Ntk const& ntk, std::vector<std::vector<node>> ranks) :
            mockturtle::depth_view<Ntk>{ntk},
            rank_pos{ntk},
            ranks{this->depth() + 1},
            max_rank_width{0}
    {
        static_assert(mockturtle::is_network_type_v<Ntk>, "Ntk is not a network type");
        static_assert(mockturtle::has_foreach_node_v<Ntk>, "Ntk does not implement the foreach_node method");
        static_assert(mockturtle::has_get_node_v<Ntk>, "Ntk does not implement the get_node method");
        static_assert(mockturtle::has_num_pis_v<Ntk>, "Ntk does not implement the num_pis method");
        static_assert(mockturtle::has_is_ci_v<Ntk>, "Ntk does not implement the is_ci method");
        static_assert(mockturtle::has_is_constant_v<Ntk>, "Ntk does not implement the is_constant method");

        init_ranks(ranks);

        add_event = Ntk::events().register_add_event([this](auto const& n) { on_add(n); });
    }

    /*! \brief Copy constructor. */
    extended_rank_view(extended_rank_view<Ntk, false> const& other) :
            mockturtle::depth_view<Ntk>(other),
            rank_pos{other.rank_pos},
            ranks{other.ranks},
            max_rank_width{other.max_rank_width}
    {
        add_event = Ntk::events().register_add_event([this](auto const& n) { on_add(n); });
    }

    extended_rank_view<Ntk, false>& operator=(extended_rank_view<Ntk, false> const& other)
    {
        /* delete the event of this network */
        Ntk::events().release_add_event(add_event);

        /* update the base class */
        this->_storage = other._storage;
        this->_events  = other._events;

        /* copy */
        rank_pos       = other.rank_pos;
        ranks          = other.ranks;
        max_rank_width = other.max_rank_width;

        /* register new event in the other network */
        add_event = Ntk::events().register_add_event([this](auto const& n) { on_add(n); });

        return *this;
    }

    ~extended_rank_view()
    {
        Ntk::events().release_add_event(add_event);
    }
    /**
     * \brief Returns the rank position of a node.
     *
     * @param n Node to get the rank position of.
     * @return Rank position of node `n`.
     */
    uint32_t rank_position(node const& n) const noexcept
    {
        assert(!this->is_constant(n) && "node must not be constant");

        return rank_pos[n];
    }

    // This function has the only purpose to enable an equivalence checking of this network
    template <typename U>
    typename std::enable_if<std::is_same<U, fiction::virtual_pi_network>::value>::type remove_virtual_input_nodes()
    {
        fiction::virtual_pi_network::remove_virtual_input_nodes();
        rank_pos.reset();  // Clear the node_map
        // Clear each vector in ranks
        for (auto& rank : ranks)
        {
            rank.clear();
        }
        // Finally, clear the ranks vector itself
        // ranks.clear();

        // Reset max_rank_width
        max_rank_width = 0;
        this->init_ranks();
        std::cout << "remove() called\n";
    }

    bool check_validity() const noexcept
    {
        for (size_t i = 0; i < ranks.size(); ++i)
        {
            const auto& rank              = ranks[i];
            uint32_t    expected_rank_pos = 0;
            for (const auto& n : rank)
            {
                // Check if the level is different from the rank level
                if (this->level(n) != i)
                {
                    return false;
                }
                // Check if the rank_pos is not in ascending order
                if (rank_pos[n] != expected_rank_pos)
                {
                    return false;
                }
                ++expected_rank_pos;  // Increment the expected rank_pos
            }
        }
        return true;
    }

    /**
     * \brief Modifies the rank by updating the nodes at the specified level with the given nodes.
     *
     * This function replaces the nodes at the specified level with the given nodes and updates the rank positions
     * accordingly.
     *
     * \param level The level at which to modify the rank.
     * \param nodes The new set of nodes for the specified level.
     */
    void modify_rank(uint32_t const level, std::vector<node>& nodes)
    {
        auto& rank = ranks[level];
        rank       = nodes;
        std::for_each(rank.cbegin(), rank.cend(), [this, i = 0u](auto const& n) mutable { rank_pos[n] = i++; });
    }
    /**
     * \brief Returns the node at a certain rank position.
     *
     * @param level Level in the network, i.e., rank to get the node from.
     * @param pos Position in the rank to get the node from.
     * @return Node at position `pos` in rank `level`.
     */
    node at_rank_position(uint32_t const level, uint32_t const pos) const noexcept
    {
        assert(level < ranks.size() && "level must be less than the number of ranks");
        assert(pos < ranks[level].size() && "pos must be less than the number of nodes in rank");

        return ranks[level][pos];
    }
    /**
     * \brief Returns the width of the widest rank in the network.
     *
     * @return Width of the widest rank in the network.
     */
    uint32_t width() const noexcept
    {
        return max_rank_width;
    }
    /**
     * \brief Swaps the positions of two nodes in the same rank.
     *
     * @param n1 First node to swap.
     * @param n2 Second node to swap.
     */
    void swap(node const& n1, node const& n2) noexcept
    {
        assert(this->level(n1) == this->level(n2) && "nodes must be in the same rank");

        auto& pos1 = rank_pos[n1];
        auto& pos2 = rank_pos[n2];

        std::swap(ranks[this->level(n1)][pos1], ranks[this->level(n2)][pos2]);
        std::swap(pos1, pos2);
    }
    /**
     * \brief Sorts the given rank according to a comparator.
     *
     * @tparam Cmp Functor type that compares two nodes. It needs to fulfill the requirements of `Compare` (named C++
     * requirement).
     * @param level The level of the rank to sort.
     * @param cmp The comparator to use.
     */
    template <typename Cmp>
    void sort_rank(uint32_t const level, Cmp const& cmp)
    {
        // level must be less than the number of ranks
        if (level < ranks.size())
        {
            auto& rank = ranks[level];

            std::sort(rank.begin(), rank.end(), cmp);
            std::for_each(rank.cbegin(), rank.cend(), [this, i = 0u](auto const& n) mutable { rank_pos[n] = i++; });
        }
    }
    /**
     * \brief Applies a given function to each node in the rank level in order.
     *
     * @tparam Fn Functor type.
     * @param level The rank to apply fn to.
     * @param fn The function to apply.
     */
    template <typename Fn>
    void foreach_node_in_rank(uint32_t const level, Fn&& fn) const
    {
        // level must be less than the number of ranks
        if (level < ranks.size())
        {
            auto const& rank = ranks[level];

            mockturtle::detail::foreach_element(rank.cbegin(), rank.cend(), std::forward<Fn>(fn));
        }
    }
    /**
     * \brief Applies a given function to each node in rank order.
     *
     * This function overrides the `foreach_node` method of the base class.
     *
     * @tparam Fn Functor type.
     * @param fn The function to apply.
     */
    template <typename Fn>
    void foreach_node(Fn&& fn) const
    {
        for (auto l = 0; l < ranks.size(); ++l)
        {
            foreach_node_in_rank(l, std::forward<Fn>(fn));
        }
    }
    /**
     * \brief Applies a given function to each gate in the rank level in order.
     *
     * @tparam Fn Functor type.
     * @param level The rank to apply fn to.
     * @param fn The function to apply.
     */
    template <typename Fn>
    void foreach_gate_in_rank(uint32_t const level, Fn&& fn) const
    {
        // level must be less than the number of ranks
        if (level < ranks.size())
        {
            auto const& rank = ranks[level];

            mockturtle::detail::foreach_element_if(
                rank.cbegin(), rank.cend(), [this](auto const& n) { return !this->is_ci(n); }, std::forward<Fn>(fn));
        }
    }
    /**
     * \brief Applies a given function to each gate in rank order.
     *
     * This function overrides the `foreach_gate` method of the base class.
     *
     * @tparam Fn Functor type.
     * @param fn The function to apply.
     */
    template <typename Fn>
    void foreach_gate(Fn&& fn) const
    {
        for (auto l = 0; l < ranks.size(); ++l)
        {
            foreach_gate_in_rank(l, std::forward<Fn>(fn));
        }
    }
    /**
     * \brief Applies a given function to each PI in rank order.
     *
     * This function overrides the `foreach_pi` method of the base class.
     *
     * @tparam Fn Functor type.
     * @param fn The function to apply.
     */
    template <typename Fn>
    void foreach_pi(Fn&& fn) const
    {
        std::vector<node> pis{};
        pis.reserve(this->num_pis());

        mockturtle::depth_view<Ntk>::foreach_pi([&pis](auto const& pi) { pis.push_back(pi); });
        std::sort(pis.begin(), pis.end(),
                  [this](auto const& n1, auto const& n2) { return rank_pos[n1] < rank_pos[n2]; });
        mockturtle::detail::foreach_element(pis.cbegin(), pis.cend(), std::forward<Fn>(fn));
    }
    /**
     * Overrides the base class method to also call the add_event on create_pi().
     *
     * @note This can (and in fact will) lead to issues if Ntk already calls add_event functions on create_pi()!
     *
     * @return Newly created PI signal.
     */
    signal create_pi()
    {
        auto const n = mockturtle::depth_view<Ntk>::create_pi();
        this->resize_levels();
        on_add(this->get_node(n));
        return n;
    }

  private:
    mockturtle::node_map<uint32_t, Ntk> rank_pos;
    std::vector<std::vector<node>>      ranks;
    uint32_t                            max_rank_width;

    std::shared_ptr<typename mockturtle::network_events<Ntk>::add_event_type> add_event;

    void insert_in_rank(node const& n) noexcept
    {
        auto& rank  = ranks[this->level(n)];
        rank_pos[n] = rank.size();
        rank.push_back(n);
        max_rank_width = std::max(max_rank_width, static_cast<uint32_t>(rank.size()));
    }

    void on_add(node const& n) noexcept
    {
        if (this->level(n) >= ranks.size())
        {
            // add sufficient ranks to store the new node
            ranks.insert(ranks.end(), this->level(n) - ranks.size() + 1, {});
        }
        rank_pos.resize();

        insert_in_rank(n);
    }

    void init_ranks() noexcept
    {
        mockturtle::depth_view<Ntk>::foreach_node(
            [this](auto const& n)
            {
                if (!this->is_constant(n))
                {
                    insert_in_rank(n);
                }
            });
    }

    void insert_in_rank(node const& n, size_t rank_level) noexcept
    {
        auto& rank  = ranks[rank_level];
        rank_pos[n] = rank.size();
        rank.push_back(n);
        max_rank_width = std::max(max_rank_width, static_cast<uint32_t>(rank.size()));
    }

    void init_ranks(std::vector<std::vector<node>> const& input_ranks) noexcept
    {
        for (size_t i = 0; i < input_ranks.size(); ++i)
        {
            auto const& rank_nodes = input_ranks[i];
            for (auto const& n : rank_nodes)
            {
                if (!this->is_constant(n))
                {
                    insert_in_rank(n, i);
                }
            }
        }
    }
};

template <class T>
extended_rank_view(T const&) -> extended_rank_view<T>;

#endif  // FICTION_VIRTUAL_RANK_VIEW_HPP