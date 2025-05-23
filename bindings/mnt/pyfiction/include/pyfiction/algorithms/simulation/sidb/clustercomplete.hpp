//
// Created by marcel on 21.11.23.
//

#ifndef PYFICTION_CLUSTERCOMPLETE_HPP
#define PYFICTION_CLUSTERCOMPLETE_HPP

#if (FICTION_ALGLIB_ENABLED)

#include "pyfiction/documentation.hpp"
#include "pyfiction/types.hpp"

#include <fiction/algorithms/simulation/sidb/clustercomplete.hpp>
#include <fiction/layouts/coordinates.hpp>

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace pyfiction
{

namespace detail
{

template <typename Lyt>
void clustercomplete(pybind11::module& m)
{
    namespace py = pybind11;

    m.def("clustercomplete", &fiction::clustercomplete<Lyt>, py::arg("lyt"),
          py::arg("params") = fiction::clustercomplete_params<>{}, DOC(fiction_clustercomplete));
}

}  // namespace detail

inline void clustercomplete(pybind11::module& m)
{
    namespace py = pybind11;

    /**
     * Report *Ground State Space* stats.
     */
    py::enum_<fiction::clustercomplete_params<>::ground_state_space_reporting>(
        m, "ground_state_space_reporting", DOC(fiction_clustercomplete_params_ground_state_space_reporting))
        .value("ON", fiction::clustercomplete_params<>::ground_state_space_reporting::ON,
               DOC(fiction_clustercomplete_params_ground_state_space_reporting_ON))
        .value("OFF", fiction::clustercomplete_params<>::ground_state_space_reporting::OFF,
               DOC(fiction_clustercomplete_params_ground_state_space_reporting_OFF));

    /**
     * ClusterComplete parameters.
     */
    py::class_<fiction::clustercomplete_params<>>(m, "clustercomplete_params", DOC(fiction_clustercomplete_params))
        .def(py::init<>())
        .def_readwrite("simulation_parameters", &fiction::clustercomplete_params<>::simulation_parameters,
                       DOC(fiction_clustercomplete_params_simulation_parameters))
        .def_readwrite("local_external_potential", &fiction::clustercomplete_params<>::local_external_potential,
                       DOC(fiction_clustercomplete_params_local_external_potential))
        .def_readwrite("global_potential", &fiction::clustercomplete_params<>::global_potential,
                       DOC(fiction_clustercomplete_params_global_potential))
        .def_readwrite("validity_witness_partitioning_max_cluster_size_gss",
                       &fiction::clustercomplete_params<>::validity_witness_partitioning_max_cluster_size_gss,
                       DOC(fiction_clustercomplete_params_validity_witness_partitioning_max_cluster_size_gss))
        .def_readwrite("num_overlapping_witnesses_limit_gss",
                       &fiction::clustercomplete_params<>::num_overlapping_witnesses_limit_gss,
                       DOC(fiction_clustercomplete_params_num_overlapping_witnesses_limit_gss))
        .def_readwrite("available_threads", &fiction::clustercomplete_params<>::available_threads,
                       DOC(fiction_clustercomplete_params_available_threads))
        .def_readwrite("report_gss_stats", &fiction::clustercomplete_params<>::report_gss_stats,
                       DOC(fiction_clustercomplete_params_report_gss_stats));

    // NOTE be careful with the order of the following calls! Python will resolve the first matching overload!

    detail::clustercomplete<py_sidb_100_lattice>(m);
    detail::clustercomplete<py_sidb_111_lattice>(m);
}

}  // namespace pyfiction

#else  // FICTION_ALGLIB_ENABLED

#include <pybind11/pybind11.h>

namespace pyfiction
{

/**
 * Disable ClusterComplete.
 */
inline void clustercomplete([[maybe_unused]] pybind11::module& m) {}

}  // namespace pyfiction

#endif  // FICTION_ALGLIB_ENABLED

#endif  // PYFICTION_CLUSTERCOMPLETE_HPP
