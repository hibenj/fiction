//
// Created by marcel on 28.11.24.
//

#ifndef FICTION_CONCEPTS_HPP
#define FICTION_CONCEPTS_HPP

#include "fiction/traits.hpp"

#include <concepts>
#include <cstdlib>
#include <type_traits>

namespace fiction
{

// ----------------------------
// FICTION-SPECIFIC CONCEPTS
// ----------------------------

template <typename Lyt>
concept has_north_c = requires(Lyt layout, coordinate<Lyt> coord) {
    { layout.north(coord) };
};

template <typename Lyt>
concept has_east_c = requires(Lyt layout, coordinate<Lyt> coord) {
    { layout.east(coord) };
};

template <typename Lyt>
concept has_south_c = requires(Lyt layout, coordinate<Lyt> coord) {
    { layout.south(coord) };
};

template <typename Lyt>
concept has_west_c = requires(Lyt layout, coordinate<Lyt> coord) {
    { layout.west(coord) };
};

template <typename Lyt>
concept has_cardinal_operations_c = has_north_c<Lyt> && has_east_c<Lyt> && has_south_c<Lyt> && has_west_c<Lyt>;

template <typename Lyt>
concept has_above_c = requires(Lyt layout, coordinate<Lyt> coord) {
    { layout.above(coord) };
};

template <typename Lyt>
concept has_below_c = requires(Lyt layout, coordinate<Lyt> coord) {
    { layout.below(coord) };
};

template <typename Lyt>
concept has_elevation_operations_c = has_above_c<Lyt> && has_below_c<Lyt>;

/**
 * Concept for a coordinate layout.
 */
 /*template <class Lyt>
struct is_coordinate_layout<
   Lyt,
   std::enable_if_t<std::conjunction_v<std::is_constructible<aspect_ratio<Lyt>, coordinate<Lyt>>,
                                       has_cardinal_operations<Lyt>, has_elevation_operations<Lyt>>,
                    std::void_t<typename Lyt::base_type, aspect_ratio<Lyt>, coordinate<Lyt>, typename Lyt::storage,
                                decltype(Lyt::max_fanin_size), decltype(Lyt::min_fanin_size),
                                decltype(std::declval<Lyt>().x()), decltype(std::declval<Lyt>().y()),
                                decltype(std::declval<Lyt>().z()), decltype(std::declval<Lyt>().area())>>>
       : std::true_type
{};*/
template <typename Lyt>
concept coordinate_layout_c = requires(Lyt coord_layout) {
    // Type requirements
    typename Lyt::base_type;
    typename Lyt::storage;
    typename aspect_ratio<Lyt>;
    typename coordinate<Lyt>;

    // Static member requirements
    { Lyt::max_fanin_size } -> std::convertible_to<std::size_t>;
    { Lyt::min_fanin_size } -> std::convertible_to<std::size_t>;

    // Member function requirements
    { coord_layout.x() } -> std::same_as<decltype(coordinate<Lyt>::x)>;
    { coord_layout.y() } -> std::same_as<decltype(coordinate<Lyt>::y)>;
    { coord_layout.z() } -> std::same_as<decltype(coordinate<Lyt>::z)>;
    { coord_layout.area() } -> std::convertible_to<double>;

    // Aspect ratio constructibility
    requires std::is_constructible_v<aspect_ratio<Lyt>, coordinate<Lyt>>;
} && has_cardinal_operations_c<Lyt> && has_elevation_operations_c<Lyt>;

template <class Lyt>
concept cell_level_layout_c = is_clocked_layout_v<Lyt> &&
                               requires(Lyt layout, cell<Lyt> c) {
                                   typename Lyt::base_type;
                                   typename Lyt::cell_type;
                                   typename Lyt::cell_mode;
                                   typename Lyt::storage;
                                   typename technology<Lyt>;
                                   { layout.get_cell_type(c) } -> std::same_as<typename Lyt::cell_type>;
                                   { layout.is_empty_cell(c) } -> std::same_as<bool>;
                                   { layout.get_cell_mode(c) } -> std::same_as<typename Lyt::cell_mode>;
                                   { layout.get_cell_name(c) } -> std::same_as<std::string>;
                               };

template <typename Lyt>
concept has_sidb_technology_c = std::is_same_v<technology<Lyt>, sidb_technology>;

template <class Lyt>
concept gate_level_layout_c =
    is_clocked_layout<Lyt>::value && mockturtle::is_network_type<Lyt>::value && requires(Lyt a) {
        typename Lyt::base_type;
        typename Lyt::storage;
    };

// Node properties

template <typename Ntk>
concept has_is_inv_c = requires(Ntk n, mockturtle::node<Ntk> node) {
    { n.is_inv(node) };
};

template <typename Ntk>
concept has_is_po_c = requires(Ntk n, mockturtle::node<Ntk> node) {
    { n.is_po(node) };
};

// ----------------------------
// MOCKTURTLE-SPECIFIC CONCEPTS
// ----------------------------

template<class Ntk>
concept has_foreach_gate_c = requires(Ntk n, mockturtle::node<Ntk> nd, uint32_t u) {
    { n.foreach_gate(nd, u) };
};

template<class Ntk>
concept has_is_constant_c = requires(Ntk n, mockturtle::node<Ntk> nd) {
    { n.is_constant(nd) };
};

template <class Ntk>
concept network_type_c = std::is_constructible_v<mockturtle::signal<Ntk>, mockturtle::node<Ntk>> && requires {
    typename Ntk::base_type;
    typename mockturtle::signal<Ntk>;
    typename mockturtle::node<Ntk>;
    typename Ntk::storage;
    Ntk::max_fanin_size;
    Ntk::min_fanin_size;
};

// Node properties

template <typename Ntk>
concept has_is_pi_c = requires(Ntk n, mockturtle::node<Ntk> node) {
    { n.is_pi(node) };
};

template <typename Ntk>
concept has_is_and_c = requires(Ntk n, mockturtle::node<Ntk> node) {
    { n.is_and(node) };
};

template <typename Ntk>
concept has_is_or_c = requires(Ntk n, mockturtle::node<Ntk> node) {
    { n.is_or(node) };
};

template <typename Ntk>
concept has_is_maj_c = requires(Ntk n, mockturtle::node<Ntk> node) {
    { n.is_or(node) };
};

}  // namespace fiction

#endif  // FICTION_CONCEPTS_HPP
