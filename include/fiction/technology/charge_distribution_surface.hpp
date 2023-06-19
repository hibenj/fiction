//
// Created by Jan Drewniok on 23.11.22.
//

#ifndef FICTION_CHARGE_DISTRIBUTION_SURFACE_HPP
#define FICTION_CHARGE_DISTRIBUTION_SURFACE_HPP

#include "fiction/algorithms/path_finding/distance.hpp"
#include "fiction/algorithms/simulation/sidb/sidb_simulation_parameters.hpp"
#include "fiction/layouts/cell_level_layout.hpp"
#include "fiction/technology/sidb_charge_state.hpp"
#include "fiction/technology/sidb_nm_position.hpp"
#include "fiction/traits.hpp"
#include "fiction/types.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <limits>
#include <optional>
#include <random>
#include <type_traits>
#include <utility>

namespace fiction
{

/**
 * A layout type to layer on top of any SiDB cell-level layout. It implements an interface to store and access
 * SiDBs' charge states.
 *
 * @tparam Lyt Cell-level layout based in SiQAD-coordinates.
 * @tparam has_sidb_charge_distribution Automatically determines whether a charge distribution interface is already
 * present.
 */
template <typename Lyt, bool has_charge_distribution_interface =
                            std::conjunction_v<has_assign_charge_state<Lyt>, has_get_charge_state<Lyt>>>
class charge_distribution_surface : public Lyt
{};

template <typename Lyt>
class charge_distribution_surface<Lyt, true> : public Lyt
{
  public:
    explicit charge_distribution_surface(const Lyt& lyt) : Lyt(lyt) {}
};

template <typename Lyt>
class charge_distribution_surface<Lyt, false> : public Lyt
{
  public:
    using charge_index_base = typename std::pair<uint64_t, uint8_t>;

    struct charge_distribution_storage
    {
      private:
        /**
         * The distance matrix is a vector of vectors storing the euclidean distance.
         */
        using distance_matrix = std::vector<std::vector<double>>;
        /**
         * The potential matrix is a vector of vectors storing the electrostatic potentials.
         */
        using potential_matrix = std::vector<std::vector<double>>;
        /**
         * It is a vector that stores the local electrostatic potential.
         */
        using local_potential = std::vector<double>;

      public:
        explicit charge_distribution_storage(const sidb_simulation_parameters& params = sidb_simulation_parameters{}) :
                phys_params{params} {};
        /**
         * Stores all physical parameters used for the simulation.
         */
        sidb_simulation_parameters phys_params{};
        /**
         * All cells that are occupied by an SiDB are stored in order.
         */
        std::vector<typename Lyt::cell> sidb_order{};
        /**
         * The SiDBs' charge states are stored. Corresponding cells are stored in `sidb_order`.
         */
        std::vector<sidb_charge_state> cell_charge{};
        /**
         * Distance between SiDBs are stored as matrix.
         */
        distance_matrix nm_dist_mat{};
        /**
         * Electrostatic potential between SiDBs are stored as matrix (here, still charge-independent).
         */
        potential_matrix pot_mat{};
        /**
         * Electrostatic potential at each SiDB position. Has to be updated when charge distribution is changed.
         */
        local_potential loc_pot{};
        /**
         * Stores the electrostatic energy of a given charge distribution.
         */
        double system_energy{0.0};
        /**
         * Labels if given charge distribution is physically valid (see https://ieeexplore.ieee.org/document/8963859).
         */
        bool validity = false;
        /**
         * Each charge distribution is assigned a unique index (first entry of pair), second one stores the base number
         * (2- or 3-state simulation).
         */
        charge_index_base charge_index{};
        /**
         * Depending on the number of SiDBs and the base number, a maximal number of possible charge distributions
         * exists.
         */
        uint64_t max_charge_index{};
    };

    using storage = std::shared_ptr<charge_distribution_storage>;

    /**
     * Standard constructor for empty layouts.
     *
     * @param params Physical parameters used for the simulation (µ_minus, base number, ...).
     * @param cs The charge state used for the initialization of all SiDBs, default is a negative charge.
     */
    explicit charge_distribution_surface(const sidb_simulation_parameters& params = sidb_simulation_parameters{},
                                         const sidb_charge_state&          cs     = sidb_charge_state::NEGATIVE) :
            Lyt(),
            strg{std::make_shared<charge_distribution_storage>(params)}
    {
        static_assert(has_siqad_coord_v<Lyt>, "Lyt is not based on SiQAD coordinates");
        static_assert(is_cell_level_layout_v<Lyt>, "Lyt is not a cell-level layout");
        static_assert(has_sidb_technology_v<Lyt>, "Lyt is not an SiDB layout");

        initialize(cs);
    }
    /**
     * Standard constructor for existing layouts.
     *
     * @param lyt The layout to be used as base.
     * @param params Physical parameters used for the simulation (µ_minus, base number, ...).
     * @param cs The charge state used for the initialization of all SiDBs, default is a negative charge.
     */
    explicit charge_distribution_surface(const Lyt&                        lyt,
                                         const sidb_simulation_parameters& params = sidb_simulation_parameters{},
                                         const sidb_charge_state&          cs     = sidb_charge_state::NEGATIVE) :
            Lyt(lyt),
            strg{std::make_shared<charge_distribution_storage>(params)}
    {
        static_assert(has_siqad_coord_v<Lyt>, "Lyt is not based on SiQAD coordinates");
        static_assert(is_cell_level_layout_v<Lyt>, "Lyt is not a cell-level layout");
        static_assert(has_sidb_technology_v<Lyt>, "Lyt is not an SiDB layout");

        initialize(cs);
    };
    /**
     * Copy constructor.
     *
     * @param lyt charge_distribution_surface
     */
    explicit charge_distribution_surface(const charge_distribution_surface<Lyt>& lyt) :
            Lyt(lyt),
            strg{std::make_shared<charge_distribution_storage>(*lyt.strg)}
    {}
    /**
     * Copy assignment operator.
     *
     * @param other charge_distribution_surface.
     */
    charge_distribution_surface& operator=(const charge_distribution_surface& other)
    {
        if (this != &other)
        {
            strg = std::make_shared<charge_distribution_storage>(*other.strg);
        }

        return *this;
    }
    /**
     * Returns all SiDB charges of the placed SiDBs as a vector.
     *
     * @return Vector of SiDB charges.
     */
    [[nodiscard]] std::vector<sidb_charge_state> get_all_sidb_charges() const noexcept
    {
        return strg->cell_charge;
    }
    /**
     * Returns the locations of all SiDBs in nm of the form `(x,y)`.
     *
     * @return Vector of SiDB nanometer positions.
     */
    [[nodiscard]] std::vector<std::pair<double, double>> get_all_sidb_locations_in_nm() const noexcept
    {
        std::vector<std::pair<double, double>> positions{};
        positions.reserve(strg->sidb_order.size());

        for (const auto& cell : strg->sidb_order)
        {
            auto pos = sidb_nm_position<Lyt>(strg->phys_params, cell);
            positions.push_back(std::make_pair(pos.first, pos.second));
        }

        return positions;
    }
    /**
     * Returns all SiDB cells.
     *
     * @return Vector of SiDB cells.
     */
    [[nodiscard]] std::vector<typename Lyt::cell> get_all_sidb_cells() const noexcept
    {
        return strg->sidb_order;
    }
    /**
     * Set the physical parameters for the simulation.
     *
     * @param params Physical parameters to be set.
     */
    void set_physical_parameters(const sidb_simulation_parameters& params) noexcept
    {
        if ((strg->phys_params.lat_a == params.lat_a) && (strg->phys_params.lat_b == params.lat_b) &&
            (strg->phys_params.lat_c == params.lat_c))
        {
            strg->phys_params         = params;
            strg->charge_index.second = params.base;
            strg->max_charge_index    = static_cast<uint64_t>(std::pow(strg->phys_params.base, this->num_cells())) - 1;
            this->update_local_potential();
            this->recompute_system_energy();
            this->validity_check();
        }
        else
        {
            strg->phys_params = params;
            this->initialize_nm_distance_matrix();
            this->initialize_potential_matrix();
            strg->charge_index.second = params.base;
            strg->max_charge_index    = static_cast<uint64_t>(std::pow(strg->phys_params.base, this->num_cells())) - 1;
            this->update_local_potential();
            this->recompute_system_energy();
            this->validity_check();
        }
    }
    /**
     * Delete the assign_cell_type function of the underlying layout.
     */
    void assign_cell_type(const typename Lyt::cell& c, const typename Lyt::cell_type& ct) = delete;
    /**
     * Check if any SiDB exhibits the given charge state.
     *
     * @param cs Charge state.
     */
    [[nodiscard]] bool charge_exists(const sidb_charge_state& cs) const noexcept
    {
        return std::any_of(strg->cell_charge.begin(), strg->cell_charge.end(),
                           [&cs](const sidb_charge_state& c) { return c == cs; });
    }
    /**
     * Retrieves the physical parameters of the simulation.
     *
     * @return sidb_simulation_parameters struct containing the physical parameters of the simulation.
     */
    [[nodiscard]] sidb_simulation_parameters get_phys_params() const noexcept
    {
        return strg->phys_params;
    }
    /**
     * This function assigns the given charge state to the cell of the layout at the specified index. It updates the
     * `cell_charge` member of `strg` object with the new charge state of the specified cell.
     *
     * @param i The index of the cell.
     * @param cs The charge state to be assigned to the cell.
     */
    void assign_charge_by_cell_index(const uint64_t i, const sidb_charge_state& cs) const noexcept
    {
        strg->cell_charge[i] = cs;
        this->charge_distribution_to_index();
    }
    /**
     * This function assigns the given charge state to the given cell of the layout.
     *
     * @param c The cell to which a charge state is to be assigned.
     * @param cs The charge state to be assigned to the cell.
     */
    void assign_charge_state(const typename Lyt::cell& c, const sidb_charge_state& cs) const noexcept
    {
        if (auto index = cell_to_index(c); index != -1)
        {
            strg->cell_charge[static_cast<uint64_t>(index)] = cs;
        }

        this->charge_distribution_to_index();
    }
    /**
     * This function assigns the given charge state to the cell (accessed by `index`) of the layout.
     *
     * @param index The index of the cell to which a charge state is to be assigned.
     * @param cs The charge state to be assigned to the cell.
     * @param update_chargeconf if set to `true`, the charge distribution index is updated after the charge distribution
     * is changed.
     */
    void assign_charge_state_by_cell_index(const uint64_t index, const sidb_charge_state& cs,
                                           const bool update_chargeconf = true) noexcept
    {
        strg->cell_charge[index] = cs;

        if (update_chargeconf)
        {
            this->charge_distribution_to_index();
        }
    }
    /**
     * Sets the charge state of all SiDBs in the layout to a given charge state.
     *
     * @param cs The charge state to be assigned to all the SiDBs.
     */
    void set_all_charge_states(const sidb_charge_state& cs) noexcept
    {
        for (uint64_t i = 0u; i < strg->cell_charge.size(); ++i)
        {
            strg->cell_charge[i] = cs;
        }

        this->charge_distribution_to_index();
    }
    /**
     * This function can be used to detect which SiDBs must be negatively charged due to their location. Important:
     * This function must be applied to a charge layout where all SiDBs are negatively initialized.
     *
     * @return Vector of indices describing which SiDBs must be negatively charged.
     */
    std::vector<int64_t> negative_sidb_detection() noexcept
    {
        std::vector<int64_t> negative_sidbs{};
        negative_sidbs.reserve(this->num_cells());
        this->foreach_cell(
            [&negative_sidbs, this](const auto& c)
            {
                if (const auto local_pot = this->get_local_potential(c); local_pot.has_value())
                {
                    if (-*local_pot + strg->phys_params.mu < -physical_constants::POP_STABILITY_ERR)
                    {
                        negative_sidbs.push_back(cell_to_index(c));
                    }
                }
            });
        return negative_sidbs;
    }
    /**
     * Returns the charge state of a cell of the layout at a given index.
     *
     * @param index The index of the cell.
     * @return The charge state of the cell at the given index.
     */
    [[nodiscard]] sidb_charge_state get_charge_state_by_index(const uint64_t index) const noexcept
    {
        if (index < (strg->cell_charge.size()))
        {
            return strg->cell_charge[index];
        }

        return sidb_charge_state::NONE;
    }
    /**
     * Returns the charge state of a given cell.
     *
     * @param c The cell.
     * @return The charge state of the given cell.
     */
    [[nodiscard]] sidb_charge_state get_charge_state(const typename Lyt::cell& c) const noexcept
    {
        if (const auto index = cell_to_index(c); index != -1)
        {
            return strg->cell_charge[static_cast<uint64_t>(index)];
        }

        return sidb_charge_state::NONE;
    }
    /**
     * Finds the index of an SiDB.
     *
     * @param c The cell to find the index of.
     * @return The index of the cell in the layout. Returns -1 if the cell is not part of the layout.
     */
    [[nodiscard]] int64_t cell_to_index(const typename Lyt::cell& c) const noexcept
    {
        if (const auto it = std::find(strg->sidb_order.cbegin(), strg->sidb_order.cend(), c);
            it != strg->sidb_order.cend())
        {
            return static_cast<int64_t>(std::distance(strg->sidb_order.cbegin(), it));
        }

        return -1;
    }
    /**
     *  Returns the distance between two cells.
     *
     *  @param c1 the first cell to compare.
     *  @param c2 the second cell to compare.
     *  @return a constexpr double representing the distance in nm between the two cells.
     */
    [[nodiscard]] double get_nm_distance_between_cells(const typename Lyt::cell& c1,
                                                       const typename Lyt::cell& c2) const noexcept
    {
        if (const auto index1 = cell_to_index(c1), index2 = cell_to_index(c2); (index1 != -1) && (index2 != -1))
        {
            return strg->nm_dist_mat[static_cast<uint64_t>(index1)][static_cast<uint64_t>(index2)];
        }

        return 0;
    }
    /**
     * Calculates and returns the distance between two cells (accessed by indices).
     *
     * @param index1 The first index.
     * @param index2 The second index.
     * @return The distance index between `index1` and `index2` (indices correspond to unique SiDBs).
     */
    [[nodiscard]] double get_nm_distance_by_indices(const uint64_t index1, const uint64_t index2) const noexcept
    {
        return strg->nm_dist_mat[index1][index2];
    }
    /**
     * Returns the chargeless electrostatic potential between two cells.
     *
     * @note If the signed electrostatic potential \f$ V_{i,j} \f$ is required, use the `get_potential_between_sidbs`
     * function.
     *
     * @param c1 The first cell.
     * @param c2 The second cell.
     * @return The chargeless electrostatic potential between `c1` and `c2`, i.e, \f$ \frac{V_{i,j}}{n_j} \f$.
     */
    [[nodiscard]] double get_chargeless_potential_between_sidbs(const typename Lyt::cell& c1,
                                                                const typename Lyt::cell& c2) const noexcept
    {
        if (const auto index1 = cell_to_index(c1), index2 = cell_to_index(c2); (index1 != -1) && (index2 != -1))
        {
            return strg->pot_mat[static_cast<uint64_t>(index1)][static_cast<uint64_t>(index2)];
        }

        return 0;
    }
    /**
     * Calculates and returns the electrostatic potential at one cell (`c1`) generated by another cell (`c2`).
     *
     * @note If the chargeless electrostatic potential \f$ \frac{V_{i,j}}{n_j} \f$ is required, use the
     * `get_chargeless_potential_between_sidbs` function.
     *
     * @param c1 The first cell.
     * @param c2 The second cell.
     * @return The electrostatic potential between `c1` and `c2`, i.e., \f$ V_{i,j} \f$.
     */
    [[nodiscard]] double get_potential_between_sidbs(const typename Lyt::cell& c1,
                                                     const typename Lyt::cell& c2) const noexcept
    {
        if (const auto index1 = cell_to_index(c1), index2 = cell_to_index(c2); (index1 != -1) && (index2 != -1))
        {
            return strg->pot_mat[static_cast<uint64_t>(index1)][static_cast<uint64_t>(index2)] *
                   charge_state_to_sign(get_charge_state(c2));
        }

        return 0;
    }
    /**
     * Calculates and returns the potential of two indices.
     *
     * @param index1 The first index.
     * @param index2 The second index.
     * @return The potential between `index1` and `index2`.
     */
    [[nodiscard]] double get_electrostatic_potential_by_indices(const uint64_t index1,
                                                                const uint64_t index2) const noexcept
    {
        return strg->pot_mat[index1][index2];
    }
    /**
     * The electrostatic potential between two cells (SiDBs) is calculated.
     *
     * @param index1 The first index.
     * @param index1 The second index.
     * @return The potential between `index1` and `index2`.
     */
    [[nodiscard]] double potential_between_sidbs_by_index(const uint64_t index1, const uint64_t index2) const noexcept
    {
        if (strg->nm_dist_mat[index1][index2] == 0)
        {
            return 0.0;
        }

        return (strg->phys_params.k / (strg->nm_dist_mat[index1][index2] * 1E-9) *
                std::exp(-strg->nm_dist_mat[index1][index2] / strg->phys_params.lambda_tf) *
                physical_constants::ELECTRIC_CHARGE);
    }
    /**
     * Calculates and returns the potential of a pair of cells based on their distance and simulation parameters.
     *
     * @param c1 The first cell.
     * @param c2 The second cell.
     * @return The potential between c1 and c2.
     */
    [[nodiscard]] double potential_between_sidbs(const typename Lyt::cell& c1,
                                                 const typename Lyt::cell& c2) const noexcept
    {
        const auto index1 = static_cast<std::size_t>(cell_to_index(c1));
        const auto index2 = static_cast<std::size_t>(cell_to_index(c2));

        return potential_between_sidbs_by_index(index1, index2);
    }
    /**
     * The function calculates the electrostatic potential for each SiDB position (local).
     */
    void update_local_potential() noexcept
    {
        strg->loc_pot.resize(this->num_cells(), 0);

        for (uint64_t i = 0u; i < strg->sidb_order.size(); ++i)
        {
            double collect = 0;
            for (uint64_t j = 0u; j < strg->sidb_order.size(); j++)
            {
                collect += strg->pot_mat[i][j] * static_cast<double>(charge_state_to_sign(strg->cell_charge[j]));
            }

            strg->loc_pot[i] = collect;
        }
    }
    /**
     * The function returns the local electrostatic potential at a given SiDB position.
     *
     * @param c The cell defining the SiDB position.
     * @return Local potential at given cell position. If there is no SiDB at the given cell, `std::nullopt` is
     * returned.
     */
    std::optional<double> get_local_potential(const typename Lyt::cell& c) const noexcept
    {
        if (const auto index = cell_to_index(c); index != -1)
        {
            return strg->loc_pot[static_cast<uint64_t>(index)];
        }

        return std::nullopt;
    }
    /**
     * The function returns the local electrostatic potential at a given index position.
     *
     * @param index The index defining the SiDB position.
     * @return local potential at given index position. If there is no SiDB at the given index (which corresponds to a
     * unique cell), `std::nullopt` is returned.
     */
    [[nodiscard]] std::optional<double> get_local_potential_by_index(const uint64_t index) const noexcept
    {
        if (index < strg->sidb_order.size())
        {
            return strg->loc_pot[index];
        }

        return std::nullopt;
    }
    /**
     * Sets the electrostatic system energy to zero. Can be used if only one SiDB is charged.
     */
    void set_system_energy_to_zero() noexcept
    {
        strg->system_energy = 0.0;
    }
    /**
     * Calculates the system's total electrostatic potential energy and stores it in the storage.
     */
    void recompute_system_energy() noexcept
    {
        double total_energy = 0;

        for (uint64_t i = 0; i < strg->loc_pot.size(); ++i)
        {
            total_energy += 0.5 * strg->loc_pot[i] * charge_state_to_sign(strg->cell_charge[i]);
        }

        strg->system_energy = total_energy;
    }
    /**
     * Return the currently stored system's total electrostatic potential energy.
     *
     * @return The system's total electrostatic potential energy.
     */
    [[nodiscard]] double get_system_energy() const noexcept
    {
        return strg->system_energy;
    }
    /**
     * The function updates the local potential and the system energy after a charge change.
     */
    void update_after_charge_change() noexcept
    {
        this->update_local_potential();
        this->recompute_system_energy();
        this->validity_check();
    }
    /**
     * The physically validity of the current charge distribution is evaluated and stored in the storage struct. A
     * charge distribution is valid if the *Population Stability* and the *Configuration Stability* is fulfilled.
     */
    void validity_check() noexcept
    {
        uint64_t population_stability_not_fulfilled_counter = 0;
        uint64_t for_loop_counter                           = 0;

        for (const auto& it : strg->loc_pot)  // this for-loop checks if the "population stability" is fulfilled.
        {
            bool valid = (((strg->cell_charge[for_loop_counter] == sidb_charge_state::NEGATIVE) &&
                           ((-it + strg->phys_params.mu) < physical_constants::POP_STABILITY_ERR)) ||
                          ((strg->cell_charge[for_loop_counter] == sidb_charge_state::POSITIVE) &&
                           ((-it + strg->phys_params.mu_p) > -physical_constants::POP_STABILITY_ERR)) ||
                          ((strg->cell_charge[for_loop_counter] == sidb_charge_state::NEUTRAL) &&
                           ((-it + strg->phys_params.mu) > -physical_constants::POP_STABILITY_ERR) &&
                           (-it + strg->phys_params.mu_p) < physical_constants::POP_STABILITY_ERR));
            for_loop_counter += 1;
            if (!valid)
            {
                strg->validity = false;  // if at least one SiDB does not fulfill the population stability, the validity
                                         // of the given charge distribution is set to "false".
                population_stability_not_fulfilled_counter += 1;
                break;
            }
        }

        if ((population_stability_not_fulfilled_counter == 0) &&
            (for_loop_counter >
             0))  // if population stability is fulfilled for all SiDBs, the "configuration stability" is checked.
        {
            const auto hop_del =
                [this](const uint64_t c1, const uint64_t c2)  // energy change when charge hops between two SiDBs.
            {
                const int dn_i = (strg->cell_charge[c1] == sidb_charge_state::NEGATIVE) ? 1 : -1;
                const int dn_j = -dn_i;

                return strg->loc_pot[c1] * dn_i + strg->loc_pot[c2] * dn_j - strg->pot_mat[c1][c2] * 1;
            };

            uint64_t hop_counter = 0;
            for (uint64_t i = 0u; i < strg->loc_pot.size(); ++i)
            {
                if (strg->cell_charge[i] == sidb_charge_state::POSITIVE)  // we do nothing with SiDB+
                {
                    continue;
                }

                for (uint64_t j = 0u; j < strg->loc_pot.size(); j++)
                {
                    if (hop_counter == 1)
                    {
                        break;
                    }

                    if (const auto e_del = hop_del(i, j);
                        (charge_state_to_sign(strg->cell_charge[j]) > charge_state_to_sign(strg->cell_charge[i])) &&
                        (e_del < -physical_constants::POP_STABILITY_ERR))  // Checks if energetically favored hops
                                                                           // exist between two SiDBs.
                    {
                        hop_counter = 1;

                        break;
                    }
                }
            }

            // If there is no jump that leads to a decrease in the potential energy of the system, the given charge
            // distribution satisfies metastability.
            strg->validity = hop_counter == 0;
        }
    }
    /**
     * Returns the currently stored validity of the present charge distribution layout.
     *
     * @returns The validity of the present charge distribution.
     */
    [[nodiscard]] bool is_physically_valid() const noexcept
    {
        return strg->validity;
    }
    /**
     * The charge distribution of the charge distribution surface is converted to a unique index. It is used to map
     * every possible charge distribution of an SiDB layout to a unique index.
     */
    void charge_distribution_to_index() const noexcept
    {
        const uint8_t base = strg->phys_params.base;

        uint64_t chargeindex = 0;
        uint64_t counter     = 0;

        for (const auto& c : strg->cell_charge)
        {
            chargeindex +=
                static_cast<uint64_t>((charge_state_to_sign(c) + 1) * std::pow(base, this->num_cells() - counter - 1));
            counter += 1;
        }

        strg->charge_index = {chargeindex, base};
    }
    /**
     * The charge index of the current charge distribution is returned.
     *
     * @return A pair with the charge index and the used base is returned.
     */
    [[nodiscard]] charge_index_base get_charge_index() const noexcept
    {
        return strg->charge_index;
    }
    /**
     *  The stored unique index is converted to the charge distribution of the charge distribution surface.
     */
    void index_to_charge_distribution() noexcept
    {
        auto       charge_quot = strg->charge_index.first;
        const auto base        = strg->charge_index.second;
        const auto num_charges = this->num_cells() - 1;
        auto       counter     = num_charges;

        while (charge_quot > 0)
        {
            const std::div_t d = std::div(static_cast<int>(charge_quot), static_cast<int>(base));
            charge_quot        = static_cast<uint64_t>(d.quot);

            this->assign_charge_state_by_cell_index(counter, sign_to_charge_state(static_cast<int8_t>(d.rem - 1)),
                                                    false);

            counter -= 1;
        }
    }
    /**
     * The charge index is increased by one, but only if it is less than the maximum charge index for the given layout.
     * If that's the case, it is increased by one and afterward, the charge configuration is updated by invoking the
     * `index_to_charge_distribution()` function.
     */
    void increase_charge_index_by_one() noexcept
    {
        if (strg->charge_index.first < strg->max_charge_index)
        {
            strg->charge_index.first += 1;
            this->index_to_charge_distribution();
            this->update_after_charge_change();
        }
    }
    /**
     * Returns the maximum index of the cell-level layout.
     *
     * @returns The maximal possible charge distribution index.
     */
    [[nodiscard]] uint64_t get_max_charge_index() const noexcept
    {
        return strg->max_charge_index;
    }
    /**
     * Assigns a certain charge state to a given index (which corresponds to a certain SiDB) and the charge distribution
     * is updated correspondingly.
     */
    void assign_charge_index(const uint64_t index) noexcept
    {
        strg->charge_index.first = index;
        this->index_to_charge_distribution();
    }
    /**
     * This function is used for the *QuickSim* algorithm (see quicksim.hpp). It gets a vector with indices representing
     * negatively charged SiDBs as input. Afterward, a distant and a neutrally charged SiDB is localized using a min-max
     * diversity algorithm. This selected SiDB is set to "negative" and the index is added to the input vector such that
     * the next iteration works correctly.
     *
     * @param alpha A parameter for the algorithm (default: 0.7).
     * @param negative_indices Vector of SiDBs indices that are already negatively charged (double occupied).
     */
    void adjacent_search(const double alpha, std::vector<uint64_t>& negative_indices) noexcept

    {
        double     dist_max     = 0;
        const auto reserve_size = this->num_cells() - negative_indices.size();

        std::vector<uint64_t> index_vector{};
        index_vector.reserve(reserve_size);
        std::vector<double> distance{};
        distance.reserve(reserve_size);

        for (uint64_t unocc = 0u; unocc < strg->cell_charge.size(); unocc++)
        {
            if (strg->cell_charge[unocc] != sidb_charge_state::NEUTRAL)
            {
                continue;
            }

            const auto dist_min =
                std::accumulate(negative_indices.begin(), negative_indices.end(), std::numeric_limits<double>::max(),
                                [&](const double acc, const uint64_t occ)
                                { return std::min(acc, this->get_nm_distance_by_indices(unocc, occ)); });

            index_vector.push_back(unocc);
            distance.push_back(dist_min);

            if (dist_min > dist_max)
            {
                dist_max = dist_min;
            }
        }

        std::vector<uint64_t> candidates{};
        candidates.reserve(reserve_size);

        for (uint64_t i = 0u; i < distance.size(); ++i)
        {
            if (distance[i] >= (alpha * dist_max))
            {
                candidates.push_back(i);
            }
        }

        if (!candidates.empty())
        {
            static std::mt19937_64                  generator(std::random_device{}());
            std::uniform_int_distribution<uint64_t> dist(0, candidates.size() - 1);
            const auto                              random_element = index_vector[candidates[dist(generator)]];
            strg->cell_charge[random_element]                      = sidb_charge_state::NEGATIVE;
            negative_indices.push_back(random_element);

            strg->system_energy += -(this->get_local_potential_by_index(random_element).value());

            for (uint64_t i = 0u; i < strg->pot_mat.size(); ++i)
            {
                strg->loc_pot[i] += -(this->get_electrostatic_potential_by_indices(i, random_element));
            }
        }
    }

  private:
    storage strg;

    /**
     * Initialization function used for the construction of the charge distribution surface.
     *
     * @param cs The charge state assigned to all SiDBs.
     */
    void initialize(const sidb_charge_state& cs = sidb_charge_state::NEGATIVE) noexcept
    {
        strg->sidb_order.reserve(this->num_cells());
        strg->cell_charge.reserve(this->num_cells());
        this->foreach_cell([this](const auto& c1) { strg->sidb_order.push_back(c1); });
        this->foreach_cell([this, &cs](const auto&) { strg->cell_charge.push_back(cs); });

        assert((((this->num_cells() < 41) && (strg->phys_params.base == 3)) ||
                ((strg->phys_params.base == 2) && (this->num_cells() < 64))) &&
               "number of SiDBs is too large");

        this->charge_distribution_to_index();
        this->initialize_nm_distance_matrix();
        this->initialize_potential_matrix();
        strg->max_charge_index =
            static_cast<uint64_t>(std::pow(static_cast<double>(strg->phys_params.base), this->num_cells()) - 1);
        this->update_local_potential();
        this->recompute_system_energy();
        this->validity_check();
    };

    /**
     * Initializes the distance matrix between all the cells of the layout.
     */
    void initialize_nm_distance_matrix() const noexcept
    {
        strg->nm_dist_mat =
            std::vector<std::vector<double>>(this->num_cells(), std::vector<double>(this->num_cells(), 0));

        for (uint64_t i = 0u; i < strg->sidb_order.size(); ++i)
        {
            for (uint64_t j = 0u; j < strg->sidb_order.size(); j++)
            {
                strg->nm_dist_mat[i][j] =
                    sidb_nanometer_distance<Lyt>(*this, strg->sidb_order[i], strg->sidb_order[j], strg->phys_params);
            }
        }
    }
    /**
     * Initializes the potential matrix between all the cells of the layout.
     */
    void initialize_potential_matrix() const noexcept
    {
        strg->pot_mat = std::vector<std::vector<double>>(this->num_cells(), std::vector<double>(this->num_cells(), 0));

        for (uint64_t i = 0u; i < strg->sidb_order.size(); ++i)
        {
            for (uint64_t j = 0u; j < strg->sidb_order.size(); j++)
            {
                strg->pot_mat[i][j] = potential_between_sidbs_by_index(i, j);
            }
        }
    }
};

template <class T>
charge_distribution_surface(const T&) -> charge_distribution_surface<T>;

template <class T>
charge_distribution_surface(const T&, const sidb_simulation_parameters&, const sidb_charge_state& cs)
    -> charge_distribution_surface<T>;

template <class T>
charge_distribution_surface(const T&, const sidb_simulation_parameters&) -> charge_distribution_surface<T>;

}  // namespace fiction

#endif  // FICTION_CHARGE_DISTRIBUTION_SURFACE_HPP
