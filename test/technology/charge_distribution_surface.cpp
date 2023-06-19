//
// Created by Jan Drewniok on 05.12.22.
//

#include <catch2/catch_template_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <fiction/layouts/cartesian_layout.hpp>
#include <fiction/layouts/cell_level_layout.hpp>
#include <fiction/layouts/clocked_layout.hpp>
#include <fiction/layouts/hexagonal_layout.hpp>
#include <fiction/technology/cell_technologies.hpp>
#include <fiction/technology/charge_distribution_surface.hpp>
#include <fiction/technology/sidb_surface.hpp>

using namespace fiction;

TEMPLATE_TEST_CASE(
    "Charge distribution surface traits and construction", "[charge-distribution-surface]",
    (cell_level_layout<sidb_technology, clocked_layout<cartesian_layout<siqad::coord_t>>>),
    (cell_level_layout<sidb_technology, clocked_layout<hexagonal_layout<siqad::coord_t, odd_row_hex>>>),
    (cell_level_layout<sidb_technology, clocked_layout<hexagonal_layout<siqad::coord_t, even_row_hex>>>),
    (cell_level_layout<sidb_technology, clocked_layout<hexagonal_layout<siqad::coord_t, odd_column_hex>>>),
    (cell_level_layout<sidb_technology, clocked_layout<hexagonal_layout<siqad::coord_t, even_column_hex>>>),

    (sidb_surface<cell_level_layout<sidb_technology, clocked_layout<cartesian_layout<siqad::coord_t>>>>),
    (sidb_surface<cell_level_layout<sidb_technology, clocked_layout<hexagonal_layout<siqad::coord_t, odd_row_hex>>>>),
    (sidb_surface<cell_level_layout<sidb_technology, clocked_layout<hexagonal_layout<siqad::coord_t, even_row_hex>>>>),
    (sidb_surface<
        cell_level_layout<sidb_technology, clocked_layout<hexagonal_layout<siqad::coord_t, odd_column_hex>>>>),
    (sidb_surface<
        cell_level_layout<sidb_technology, clocked_layout<hexagonal_layout<siqad::coord_t, even_column_hex>>>>))
{
    REQUIRE(is_cell_level_layout_v<TestType>);
    CHECK(!has_assign_charge_state_v<TestType>);
    CHECK(!has_get_charge_state_v<TestType>);

    TestType lyt{};

    using charge_layout = charge_distribution_surface<TestType>;
    CHECK(is_cell_level_layout_v<charge_layout>);
    CHECK(has_assign_charge_state_v<charge_layout>);
    CHECK(has_get_charge_state_v<charge_layout>);

    const charge_layout defect_lyt{};
    const charge_layout defect_lyt_from_lyt{lyt};

    using charge_charge_layout = charge_distribution_surface<charge_layout>;
    CHECK(is_cell_level_layout_v<charge_charge_layout>);
    CHECK(has_assign_charge_state_v<charge_charge_layout>);
    CHECK(has_get_charge_state_v<charge_charge_layout>);
}

TEMPLATE_TEST_CASE(
    "Assign and delete charge states", "[charge-distribution-surface]",
    (cell_level_layout<sidb_technology, clocked_layout<cartesian_layout<siqad::coord_t>>>),
    (cell_level_layout<sidb_technology, clocked_layout<hexagonal_layout<siqad::coord_t, odd_row_hex>>>),
    (cell_level_layout<sidb_technology, clocked_layout<hexagonal_layout<siqad::coord_t, even_row_hex>>>),
    (cell_level_layout<sidb_technology, clocked_layout<hexagonal_layout<siqad::coord_t, odd_column_hex>>>),
    (cell_level_layout<sidb_technology, clocked_layout<hexagonal_layout<siqad::coord_t, even_column_hex>>>),

    (sidb_surface<cell_level_layout<sidb_technology, clocked_layout<cartesian_layout<siqad::coord_t>>>>),
    (sidb_surface<cell_level_layout<sidb_technology, clocked_layout<hexagonal_layout<siqad::coord_t, odd_row_hex>>>>),
    (sidb_surface<cell_level_layout<sidb_technology, clocked_layout<hexagonal_layout<siqad::coord_t, even_row_hex>>>>),
    (sidb_surface<
        cell_level_layout<sidb_technology, clocked_layout<hexagonal_layout<siqad::coord_t, odd_column_hex>>>>),
    (sidb_surface<
        cell_level_layout<sidb_technology, clocked_layout<hexagonal_layout<siqad::coord_t, even_column_hex>>>>))

{
    TestType lyt{{11, 11}};

    SECTION("assign and read out charge states")
    {
        // assign SiDBs and charge states to three different cells and read the charge state
        lyt.assign_cell_type({5, 4}, TestType::cell_type::NORMAL);
        lyt.assign_cell_type({5, 5}, TestType::cell_type::NORMAL);
        lyt.assign_cell_type({5, 6}, TestType::cell_type::NORMAL);
        charge_distribution_surface charge_layout{lyt, sidb_simulation_parameters{}};
        charge_layout.assign_charge_state({5, 4}, sidb_charge_state::POSITIVE);
        charge_layout.assign_charge_state({5, 5}, sidb_charge_state::NEUTRAL);
        charge_layout.assign_charge_state({5, 6}, sidb_charge_state::NEGATIVE);
        CHECK(charge_layout.get_charge_state({5, 4}) == sidb_charge_state::POSITIVE);
        CHECK(charge_layout.get_charge_state({5, 5}) == sidb_charge_state::NEUTRAL);
        CHECK(charge_layout.get_charge_state({5, 6}) == sidb_charge_state::NEGATIVE);

        CHECK(charge_layout.get_charge_state({7, 6}) == sidb_charge_state::NONE);

        charge_layout.assign_charge_state_by_cell_index(0, sidb_charge_state::NEUTRAL);
        charge_layout.assign_charge_state_by_cell_index(1, sidb_charge_state::POSITIVE);
        charge_layout.assign_charge_state_by_cell_index(2, sidb_charge_state::POSITIVE);
        CHECK(charge_layout.get_charge_state_by_index(0) == sidb_charge_state::NEUTRAL);
        CHECK(charge_layout.get_charge_state_by_index(1) == sidb_charge_state::POSITIVE);
        CHECK(charge_layout.get_charge_state_by_index(2) == sidb_charge_state::POSITIVE);

        // check if charge state for empty cell is 'NONE'
        CHECK(charge_layout.get_charge_state_by_index(3) == sidb_charge_state::NONE);
    }

    SECTION("assign different charge states to the same cell")
    {
        // check if the charge state can be overwritten
        lyt.assign_cell_type({5, 1}, TestType::cell_type::NORMAL);
        charge_distribution_surface charge_layout{lyt, sidb_simulation_parameters{}};
        charge_layout.assign_charge_state({5, 1}, sidb_charge_state::NEUTRAL);
        charge_layout.assign_charge_state({5, 1}, sidb_charge_state::NONE);
        charge_layout.assign_charge_state({5, 1}, sidb_charge_state::POSITIVE);
        charge_layout.assign_charge_state({5, 1}, sidb_charge_state::NEGATIVE);
        CHECK(charge_layout.get_charge_state({5, 1}) == sidb_charge_state::NEGATIVE);
    }

    SECTION("overwrite an assigned charge state")
    {
        // assign SiDBs and charge states to three different cells
        lyt.assign_cell_type({5, 4}, TestType::cell_type::NORMAL);
        lyt.assign_cell_type({5, 5}, TestType::cell_type::NORMAL);
        lyt.assign_cell_type({5, 6}, TestType::cell_type::NORMAL);
        charge_distribution_surface charge_layout{lyt, sidb_simulation_parameters{}};
        charge_layout.assign_charge_state({5, 4}, sidb_charge_state::POSITIVE);
        charge_layout.assign_charge_state({5, 5}, sidb_charge_state::NEUTRAL);
        charge_layout.assign_charge_state({5, 6}, sidb_charge_state::NEGATIVE);

        // all SiDBs' charge states are set to positive
        charge_layout.set_all_charge_states(sidb_charge_state::POSITIVE);

        // calculate potential between two sidbs (charge sign not included)
        CHECK(charge_layout.potential_between_sidbs({5, 4}, {5, 5}) > 0);
        CHECK_THAT(charge_layout.potential_between_sidbs({5, 4}, {5, 4}), Catch::Matchers::WithinAbs(0.0, 0.00001));
        CHECK(charge_layout.potential_between_sidbs({5, 4}, {5, 6}) > 0);
        CHECK(charge_layout.potential_between_sidbs({5, 5}, {5, 6}) > 0);
        CHECK_THAT(charge_layout.potential_between_sidbs({5, 6}, {5, 5}) -
                       charge_layout.potential_between_sidbs({5, 5}, {5, 6}),
                   Catch::Matchers::WithinAbs(0.0, 0.00001));
        // read SiDBs' charge states
        CHECK(charge_layout.get_charge_state({5, 4}) == sidb_charge_state::POSITIVE);
        CHECK(charge_layout.get_charge_state({5, 5}) == sidb_charge_state::POSITIVE);
        CHECK(charge_layout.get_charge_state({5, 6}) == sidb_charge_state::POSITIVE);
        CHECK(charge_layout.get_charge_state({5, 1}) == sidb_charge_state::NONE);

        charge_layout.set_all_charge_states(sidb_charge_state::POSITIVE);

        // all SiDBs' charge states are set to neutral
        charge_layout.set_all_charge_states(sidb_charge_state::NEUTRAL);

        // read SiDBs' charge states
        CHECK(charge_layout.get_charge_state({5, 4}) == sidb_charge_state::NEUTRAL);
        CHECK(charge_layout.get_charge_state({5, 5}) == sidb_charge_state::NEUTRAL);
        CHECK(charge_layout.get_charge_state({5, 6}) == sidb_charge_state::NEUTRAL);
        CHECK(charge_layout.get_charge_state({5, 1}) == sidb_charge_state::NONE);

        charge_layout.set_all_charge_states(sidb_charge_state::NEUTRAL);

        // all SiDBs' charge states are set to negative
        charge_layout.set_all_charge_states(sidb_charge_state::NEGATIVE);

        // read SiDBs' charge states
        CHECK(charge_layout.get_charge_state({5, 4}) == sidb_charge_state::NEGATIVE);
        CHECK(charge_layout.get_charge_state({5, 5}) == sidb_charge_state::NEGATIVE);
        CHECK(charge_layout.get_charge_state({5, 6}) == sidb_charge_state::NEGATIVE);
        CHECK(charge_layout.get_charge_state({5, 1}) == sidb_charge_state::NONE);
        //
        charge_layout.set_all_charge_states(sidb_charge_state::NEGATIVE);
    }

    SECTION("overwrite the charge state")
    {
        // assign SiDBs and charge states to three different cells
        lyt.assign_cell_type({5, 4}, TestType::cell_type::NORMAL);
        lyt.assign_cell_type({5, 5}, TestType::cell_type::NORMAL);
        lyt.assign_cell_type({5, 6}, TestType::cell_type::NORMAL);

        charge_distribution_surface charge_layout{lyt, sidb_simulation_parameters{}};

        charge_layout.assign_charge_state({5, 4}, sidb_charge_state::POSITIVE);
        charge_layout.assign_charge_state({5, 5}, sidb_charge_state::NEUTRAL);
        charge_layout.assign_charge_state({5, 6}, sidb_charge_state::NEGATIVE);

        // remove previously assigned charge states
        charge_layout.assign_charge_state({5, 4}, sidb_charge_state::NONE);
        charge_layout.assign_charge_state({5, 5}, sidb_charge_state::NONE);
        charge_layout.assign_charge_state({5, 6}, sidb_charge_state::POSITIVE);
        CHECK(charge_layout.get_charge_state({5, 4}) == sidb_charge_state::NONE);
        CHECK(charge_layout.get_charge_state({5, 5}) == sidb_charge_state::NONE);
        CHECK(charge_layout.get_charge_state({5, 6}) == sidb_charge_state::POSITIVE);
    }

    SECTION("Distance matrix")
    {
        lyt.assign_cell_type({0, 0, 0}, TestType::cell_type::NORMAL);
        lyt.assign_cell_type({1, 0, 0}, TestType::cell_type::NORMAL);
        lyt.assign_cell_type({1, 1, 1}, TestType::cell_type::NORMAL);

        charge_distribution_surface charge_layout{lyt, sidb_simulation_parameters{}};

        CHECK_THAT(charge_layout.get_nm_distance_between_cells({0, 0, 0}, {0, 0, 0}),
                   Catch::Matchers::WithinAbs(0.0, 0.00001));
        CHECK_THAT(charge_layout.get_nm_distance_between_cells({0, 0, 0}, {1, 0, 0}),
                   Catch::Matchers::WithinAbs(sidb_simulation_parameters{}.lat_a * 0.1, 0.00001));
        CHECK_THAT(charge_layout.get_nm_distance_between_cells({1, 0, 0}, {0, 0, 0}),
                   Catch::Matchers::WithinAbs(sidb_simulation_parameters{}.lat_a * 0.1, 0.00001));
        CHECK_THAT(charge_layout.get_nm_distance_between_cells({1, 0, 0}, {1, 0, 0}),
                   Catch::Matchers::WithinAbs(0.0, 0.00001));
        CHECK_THAT(charge_layout.get_nm_distance_between_cells({0, 0, 0}, {1, 1, 1}),
                   Catch::Matchers::WithinAbs(
                       std::hypot(sidb_simulation_parameters{}.lat_a * 0.1,
                                  sidb_simulation_parameters{}.lat_b * 0.1 + sidb_simulation_parameters{}.lat_c * 0.1),
                       0.00001));
        CHECK_THAT(charge_layout.get_nm_distance_between_cells({1, 1, 1}, {1, 1, 1}),
                   Catch::Matchers::WithinAbs(0.0, 0.00001));
    }

    SECTION("Potential matrix")
    {
        lyt.assign_cell_type({0, 0, 0}, TestType::cell_type::NORMAL);
        lyt.assign_cell_type({1, 8, 0}, TestType::cell_type::NORMAL);
        lyt.assign_cell_type({1, 10, 1}, TestType::cell_type::NORMAL);

        charge_distribution_surface charge_layout{lyt, sidb_simulation_parameters{}};

        CHECK_THAT(charge_layout.get_chargeless_potential_between_sidbs({2, 8, 0}, {2, 10, 1}),
                   Catch::Matchers::WithinAbs(0.0, 0.00001));
        CHECK_THAT(charge_layout.get_chargeless_potential_between_sidbs({0, 0, 0}, {0, 0, 0}),
                   Catch::Matchers::WithinAbs(0.0, 0.00001));
        CHECK_THAT(charge_layout.get_chargeless_potential_between_sidbs({1, 8, 0}, {1, 8, 0}),
                   Catch::Matchers::WithinAbs(0.0, 0.00001));
        CHECK_THAT(charge_layout.get_chargeless_potential_between_sidbs({1, 10, 1}, {1, 10, 1}),
                   Catch::Matchers::WithinAbs(0.0, 0.00001));
        CHECK_THAT(charge_layout.get_chargeless_potential_between_sidbs({1, 8, 0}, {0, 0, 0}),
                   Catch::Matchers::WithinAbs(0.0121934043, 0.00001));
        CHECK_THAT(std::abs(charge_layout.get_chargeless_potential_between_sidbs({0, 0, 0}, {1, 10, 1}) -
                            charge_layout.get_chargeless_potential_between_sidbs({1, 10, 1}, {0, 0, 0})),
                   Catch::Matchers::WithinAbs(0.0, 0.00001));

        CHECK(charge_layout.get_chargeless_potential_between_sidbs({0, 0, 0}, {1, 8, 0}) >
              charge_layout.get_chargeless_potential_between_sidbs({1, 10, 1}, {0, 0, 0}));
    }

    SECTION("Local Potential")
    {
        lyt.assign_cell_type({0, 0, 0}, TestType::cell_type::NORMAL);
        lyt.assign_cell_type({1, 8, 0}, TestType::cell_type::NORMAL);
        lyt.assign_cell_type({1, 10, 1}, TestType::cell_type::NORMAL);
        charge_distribution_surface charge_layout{lyt, sidb_simulation_parameters{}};
        charge_layout.assign_charge_state({0, 0, 0}, sidb_charge_state::POSITIVE);
        charge_layout.assign_charge_state({1, 8, 0}, sidb_charge_state::POSITIVE);
        charge_layout.assign_charge_state({1, 10, 1}, sidb_charge_state::POSITIVE);

        charge_layout.update_local_potential();

        charge_layout.foreach_cell(
            [&charge_layout](const auto& c)
            {
                const auto p = charge_layout.get_local_potential(c);
                REQUIRE(p.has_value());
                CHECK(*p > 0.0);
            });

        charge_layout.assign_charge_state({0, 0, 0}, sidb_charge_state::NEGATIVE);
        charge_layout.assign_charge_state({1, 8, 0}, sidb_charge_state::NEGATIVE);
        charge_layout.assign_charge_state({1, 10, 1}, sidb_charge_state::NEGATIVE);

        charge_layout.update_local_potential();

        charge_layout.foreach_cell(
            [&charge_layout](const auto& c)
            {
                const auto p = charge_layout.get_local_potential(c);
                REQUIRE(p.has_value());
                CHECK(*p < 0.0);
            });

        charge_layout.set_all_charge_states(sidb_charge_state::NEUTRAL);

        charge_layout.update_local_potential();

        charge_layout.foreach_cell(
            [&charge_layout](const auto& c)
            {
                const auto p = charge_layout.get_local_potential(c);
                REQUIRE(p.has_value());
                CHECK_THAT(*p, Catch::Matchers::WithinAbs(0.0, 0.00001));
            });
    }

    SECTION("Electrostatic potential energy of the charge configuration")
    {
        lyt.assign_cell_type({0, 0, 0}, TestType::cell_type::NORMAL);
        lyt.assign_cell_type({1, 0, 0}, TestType::cell_type::NORMAL);
        lyt.assign_cell_type({1, 1, 1}, TestType::cell_type::NORMAL);

        charge_distribution_surface charge_layout{lyt, sidb_simulation_parameters{}};

        charge_layout.assign_charge_state({0, 0, 0}, sidb_charge_state::NEGATIVE);
        charge_layout.assign_charge_state({1, 0, 0}, sidb_charge_state::NEGATIVE);
        charge_layout.assign_charge_state({1, 1, 1}, sidb_charge_state::NEGATIVE);

        // system energy is zero when all SiDBs are positively charged.
        charge_layout.update_local_potential();
        charge_layout.recompute_system_energy();
        CHECK(charge_layout.get_system_energy() > 0);

        // system energy is zero when all SiDBs are neutrally charged.
        charge_layout.set_all_charge_states(sidb_charge_state::NEUTRAL);
        charge_layout.update_local_potential();
        charge_layout.recompute_system_energy();
        CHECK_THAT(charge_layout.get_system_energy(), Catch::Matchers::WithinAbs(0.0, 0.00001));

        // system energy is zero when all SiDBs are positively charged.
        charge_layout.set_all_charge_states(sidb_charge_state::POSITIVE);
        charge_layout.update_local_potential();
        charge_layout.recompute_system_energy();
        CHECK(charge_layout.get_system_energy() > 0);
    }
    //
    SECTION("Physical validity check, far distance of SIDBs, all NEGATIVE")
    {

        TestType layout{{11, 11}};
        layout.assign_cell_type({0, 0, 0}, TestType::cell_type::NORMAL);
        layout.assign_cell_type({0, 2, 0}, TestType::cell_type::NORMAL);
        layout.assign_cell_type({4, 1, 1}, TestType::cell_type::NORMAL);

        charge_distribution_surface charge_layout_five{layout, sidb_simulation_parameters{}};
        CHECK(charge_layout_five.get_charge_state({0, 0, 0}) == sidb_charge_state::NEGATIVE);
        CHECK(charge_layout_five.get_charge_state({0, 2, 0}) == sidb_charge_state::NEGATIVE);
        CHECK(charge_layout_five.get_charge_state({4, 1, 1}) == sidb_charge_state::NEGATIVE);

        charge_layout_five.update_local_potential();
        charge_layout_five.recompute_system_energy();
        charge_layout_five.validity_check();
        CHECK(charge_layout_five.get_charge_index().first == 0);

        charge_layout_five.assign_charge_state({4, 1, 1}, sidb_charge_state::POSITIVE);
        CHECK(charge_layout_five.get_charge_state({4, 1, 1}) == sidb_charge_state::POSITIVE);
        charge_layout_five.charge_distribution_to_index();
        CHECK(charge_layout_five.get_charge_index().first == 2);

        charge_layout_five.increase_charge_index_by_one();
        CHECK(charge_layout_five.get_charge_index().first == 3);
        charge_layout_five.increase_charge_index_by_one();
        CHECK(charge_layout_five.get_charge_index().first == 4);
        charge_layout_five.increase_charge_index_by_one();
        CHECK(charge_layout_five.get_charge_index().first == 5);
        charge_layout_five.increase_charge_index_by_one();
        CHECK(charge_layout_five.get_charge_index().first == 6);
        charge_layout_five.increase_charge_index_by_one();
        CHECK(charge_layout_five.get_charge_index().first == 7);
        charge_layout_five.increase_charge_index_by_one();
        CHECK(charge_layout_five.get_charge_index().first == 8);
        charge_layout_five.increase_charge_index_by_one();
        CHECK(charge_layout_five.get_charge_index().first == 9);
        charge_layout_five.increase_charge_index_by_one();
        CHECK(charge_layout_five.get_charge_index().first == 10);

        CHECK(charge_layout_five.get_charge_state({0, 0, 0}) == sidb_charge_state::NEUTRAL);
        CHECK(charge_layout_five.get_charge_state({0, 2, 0}) == sidb_charge_state::NEGATIVE);
        CHECK(charge_layout_five.get_charge_state({4, 1, 1}) == sidb_charge_state::NEUTRAL);
    }
    //
    SECTION("Physical validity check, small distance, not all can be negatively charged anymore")
    {
        lyt.assign_cell_type({1, 0, 0}, TestType::cell_type::NORMAL);
        lyt.assign_cell_type({0, 2, 0}, TestType::cell_type::NORMAL);
        lyt.assign_cell_type({0, 2, 1}, TestType::cell_type::NORMAL);

        charge_distribution_surface charge_layout{lyt, sidb_simulation_parameters{}};
        charge_layout.assign_charge_state({1, 0, 0}, sidb_charge_state::NEGATIVE);
        charge_layout.assign_charge_state({0, 2, 0}, sidb_charge_state::NEGATIVE);
        charge_layout.assign_charge_state({0, 2, 1}, sidb_charge_state::NEGATIVE);

        // closely arranged SiDBs cannot be all negatively charged
        charge_layout.update_local_potential();
        charge_layout.recompute_system_energy();
        charge_layout.validity_check();
        CHECK(!charge_layout.is_physically_valid());
    }

    SECTION("increase charge index")
    {
        TestType                         lyt_new{{11, 11}};
        const sidb_simulation_parameters params{3, -0.32};

        lyt_new.assign_cell_type({0, 0, 1}, TestType::cell_type::NORMAL);
        lyt_new.assign_cell_type({1, 3, 0}, TestType::cell_type::NORMAL);
        lyt_new.assign_cell_type({10, 5, 1}, TestType::cell_type::NORMAL);

        charge_distribution_surface charge_layout_new{lyt_new, params};
        const auto                  negative_sidbs = charge_layout_new.negative_sidb_detection();
        REQUIRE(negative_sidbs.size() == 3);
        CHECK(negative_sidbs[0] == 0);
        CHECK(negative_sidbs[1] == 1);
        CHECK(negative_sidbs[2] == 2);
        CHECK(charge_layout_new.get_charge_index().first == 0);

        charge_layout_new.set_all_charge_states(sidb_charge_state::POSITIVE);
        CHECK(charge_layout_new.get_charge_state({0, 0, 1}) == sidb_charge_state::POSITIVE);
        CHECK(charge_layout_new.get_charge_state({1, 3, 0}) == sidb_charge_state::POSITIVE);
        CHECK(charge_layout_new.get_charge_state({10, 5, 1}) == sidb_charge_state::POSITIVE);

        charge_layout_new.charge_distribution_to_index();
        CHECK(charge_layout_new.get_charge_index().first == 26);

        charge_layout_new.set_all_charge_states(sidb_charge_state::NEUTRAL);

        charge_layout_new.charge_distribution_to_index();
        CHECK(charge_layout_new.get_charge_index().first == 13);

        charge_layout_new.increase_charge_index_by_one();
        charge_layout_new.charge_distribution_to_index();
        CHECK(charge_layout_new.get_charge_index().first == 14);
        charge_layout_new.increase_charge_index_by_one();
        CHECK(charge_layout_new.get_charge_index().first == 15);
    }

    SECTION("using chargeless and normal potential function")
    {
        TestType                         lyt_new{{11, 11}};
        const sidb_simulation_parameters params{3, -0.32, 5.0, 3.84, 7.68, 2.25};

        lyt_new.assign_cell_type({0, 0, 1}, TestType::cell_type::NORMAL);
        lyt_new.assign_cell_type({1, 3, 0}, TestType::cell_type::NORMAL);
        lyt_new.assign_cell_type({10, 5, 1}, TestType::cell_type::NORMAL);

        charge_distribution_surface charge_layout_new{lyt_new, params};

        charge_layout_new.assign_charge_state({0, 0, 1}, sidb_charge_state::NEGATIVE);
        charge_layout_new.assign_charge_state({1, 3, 0}, sidb_charge_state::POSITIVE);
        charge_layout_new.assign_charge_state({10, 5, 1}, sidb_charge_state::NEUTRAL);

        CHECK(charge_layout_new.get_charge_state({0, 0, 1}) == sidb_charge_state::NEGATIVE);
        CHECK(charge_layout_new.get_charge_state({1, 3, 0}) == sidb_charge_state::POSITIVE);
        CHECK(charge_layout_new.get_charge_state({10, 5, 1}) == sidb_charge_state::NEUTRAL);

        CHECK(charge_layout_new.get_chargeless_potential_between_sidbs({0, 0, 1}, {1, 3, 0}) > 0);
        CHECK(charge_layout_new.get_potential_between_sidbs({0, 0, 1}, {1, 3, 0}) > 0);

        CHECK(charge_layout_new.get_chargeless_potential_between_sidbs({0, 0, 1}, {10, 5, 1}) > 0);
        CHECK_THAT(charge_layout_new.get_potential_between_sidbs({0, 0, 1}, {10, 5, 1}),
                   Catch::Matchers::WithinAbs(0.0, 0.000001));

        CHECK(charge_layout_new.get_chargeless_potential_between_sidbs({10, 5, 1}, {0, 0, 1}) > 0);
        CHECK(charge_layout_new.get_potential_between_sidbs({10, 5, 1}, {0, 0, 1}) < 0);

        CHECK_THAT(charge_layout_new.get_potential_between_sidbs({10, 5, 1}, {0, 0, 1}) +
                       charge_layout_new.get_chargeless_potential_between_sidbs({10, 5, 1}, {0, 0, 1}),
                   Catch::Matchers::WithinAbs(0.0, 0.000001));

        CHECK_THAT(charge_layout_new.get_potential_between_sidbs({0, 0, 1}, {1, 3, 0}) -
                       charge_layout_new.get_chargeless_potential_between_sidbs({0, 0, 1}, {1, 3, 0}),
                   Catch::Matchers::WithinAbs(0.0, 0.000001));
    }
}
