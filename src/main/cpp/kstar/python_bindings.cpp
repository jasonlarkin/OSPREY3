#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/functional.h>

#include "kstar_workflow.hpp"
#include "energy_matrix_loader.hpp"
#include "partition_function.hpp"

namespace py = pybind11;
using namespace osprey::kstar;

// Bind double specialization (primary use case)
void bind_kstar_double(py::module_& m) {
    // PartitionFunctionResult
    py::class_<PartitionFunctionResult<double>>(m, "PartitionFunctionResult")
        .def_readonly("lower_bound", &PartitionFunctionResult<double>::lower_bound)
        .def_readonly("upper_bound", &PartitionFunctionResult<double>::upper_bound)
        .def_readonly("delta", &PartitionFunctionResult<double>::delta)
        .def_readonly("num_confs", &PartitionFunctionResult<double>::num_confs)
        .def_readonly("converged", &PartitionFunctionResult<double>::converged)
        .def("__repr__", [](const PartitionFunctionResult<double>& r) {
            return py::str("PartitionFunctionResult(lower={:.6f}, upper={:.6f}, delta={:.6f}, num_confs={}, converged={})")
                .format(r.lower_bound, r.upper_bound, r.delta, r.num_confs, r.converged);
        });

    // PartitionFunctionTraceStep
    py::class_<PartitionFunctionTraceStep<double>>(m, "PartitionFunctionTraceStep")
        .def_readonly("iter", &PartitionFunctionTraceStep<double>::iter)
        .def_readonly("level", &PartitionFunctionTraceStep<double>::level)
        .def_readonly("open_size", &PartitionFunctionTraceStep<double>::open_size)
        .def_readonly("num_confs_evaluated", &PartitionFunctionTraceStep<double>::num_confs_evaluated)
        .def_readonly("g_score", &PartitionFunctionTraceStep<double>::g_score)
        .def_readonly("h_score", &PartitionFunctionTraceStep<double>::h_score)
        .def_readonly("f_score", &PartitionFunctionTraceStep<double>::f_score)
        .def_readonly("log10_q_lower", &PartitionFunctionTraceStep<double>::log10_q_lower)
        .def_readonly("log10_q_upper", &PartitionFunctionTraceStep<double>::log10_q_upper)
        .def_readonly("delta", &PartitionFunctionTraceStep<double>::delta)
        .def_readonly("converged", &PartitionFunctionTraceStep<double>::converged)
        .def_readonly("is_leaf", &PartitionFunctionTraceStep<double>::is_leaf)
        .def_readonly("assignments", &PartitionFunctionTraceStep<double>::assignments);

    // KStarPfuncTriplet (exposed via KStarWorkflowResult)
    py::class_<KStarPfuncTriplet<double>>(m, "KStarPfuncTriplet")
        .def_readonly("protein", &KStarPfuncTriplet<double>::protein)
        .def_readonly("ligand", &KStarPfuncTriplet<double>::ligand)
        .def_readonly("complex", &KStarPfuncTriplet<double>::complex);

    // KStarWorkflowResult
    py::class_<KStarWorkflowResult<double>>(m, "KStarWorkflowResult")
        .def_readonly("pfuncs", &KStarWorkflowResult<double>::pfuncs)
        .def_readonly("log10_value", &KStarWorkflowResult<double>::log10_value)
        .def_readonly("log10_lower_bound", &KStarWorkflowResult<double>::log10_lower_bound)
        .def_readonly("log10_upper_bound", &KStarWorkflowResult<double>::log10_upper_bound)
        .def_readonly("converged", &KStarWorkflowResult<double>::converged)
        .def("__repr__", [](const KStarWorkflowResult<double>& r) {
            return py::str("KStarWorkflowResult(log10_value={:.6f}, bounds=[{:.6f}, {:.6f}], converged={})")
                .format(r.log10_value, r.log10_lower_bound, r.log10_upper_bound, r.converged);
        });

    // EnergyMatrix
    py::class_<EnergyMatrix<double>>(m, "EnergyMatrix")
        .def(py::init<>())
        .def("get_num_positions", &EnergyMatrix<double>::getNumPositions)
        .def("get_num_confs_at_pos", &EnergyMatrix<double>::getNumConfsAtPos,
             py::arg("pos"), "Get number of conformations at a specific position")
        .def("get_const_term", &EnergyMatrix<double>::getConstTerm)
        .def("get_one_body", &EnergyMatrix<double>::getOneBody,
             py::arg("pos"), py::arg("conf"),
             "Get one-body energy for (pos, conf)")
        .def("get_pairwise", &EnergyMatrix<double>::getPairwise,
             py::arg("pos1"), py::arg("conf1"), py::arg("pos2"), py::arg("conf2"),
             "Get pairwise energy for (pos1,conf1,pos2,conf2)")
        .def("compute_energy", &EnergyMatrix<double>::computeEnergy,
             py::arg("conf"),
             "Compute total energy for a conformation vector (length = num_positions)");

    // PartitionFunctionMethod enum
    py::enum_<PartitionFunctionMethod>(m, "PartitionFunctionMethod")
        .value("AStar", PartitionFunctionMethod::AStar)
        .value("GradientDescent", PartitionFunctionMethod::GradientDescent);

    // AStarVariant enum
    py::enum_<AStarVariant>(m, "AStarVariant")
        .value("Baseline", AStarVariant::Baseline)
        .value("Fast", AStarVariant::Fast);

    // PartitionFunction::ComputeOptions
    py::class_<PartitionFunction<double>::ComputeOptions>(m, "PartitionFunctionOptions")
        .def(py::init<>())
        .def_readwrite("allow_exact_enumeration", &PartitionFunction<double>::ComputeOptions::allow_exact_enumeration)
        .def_readwrite("astar_variant", &PartitionFunction<double>::ComputeOptions::astar_variant)
        .def_readwrite("trace_max_steps", &PartitionFunction<double>::ComputeOptions::trace_max_steps)
        .def_readwrite("trace_capture_assignments", &PartitionFunction<double>::ComputeOptions::trace_capture_assignments);

    // PartitionFunction (optional, for advanced use)
    py::class_<PartitionFunction<double>>(m, "PartitionFunction")
        .def(py::init<>())
        .def("compute", [](PartitionFunction<double>& pfunc,
                           const EnergyMatrix<double>& emat,
                           double epsilon) {
            return pfunc.compute(emat, epsilon);
        }, py::arg("energy_matrix"), py::arg("epsilon"),
           "Compute partition function with default method (AStar)")
        .def("compute", [](PartitionFunction<double>& pfunc,
                           const EnergyMatrix<double>& emat,
                           double epsilon,
                           PartitionFunctionMethod method) {
            return pfunc.compute(emat, epsilon, method);
        }, py::arg("energy_matrix"), py::arg("epsilon"), py::arg("method"),
           "Compute partition function with specified method")
        .def("compute", [](PartitionFunction<double>& pfunc,
                           const EnergyMatrix<double>& emat,
                           double epsilon,
                           PartitionFunctionMethod method,
                           PartitionFunction<double>::ComputeOptions options) {
            return pfunc.compute(emat, epsilon, method, options);
        }, py::arg("energy_matrix"), py::arg("epsilon"), py::arg("method"), py::arg("options"),
           "Compute partition function with method and options");

    // KStarWorkflow
    py::class_<KStarWorkflow<double>>(m, "KStarWorkflow")
        .def(py::init<>())
        .def("compute", [](const KStarWorkflow<double>& workflow,
                           const EnergyMatrix<double>& protein,
                           const EnergyMatrix<double>& ligand,
                           const EnergyMatrix<double>& complex,
                           double epsilon) {
            return workflow.compute(protein, ligand, complex, epsilon, PartitionFunctionMethod::AStar, PartitionFunction<double>::ComputeOptions{});
        }, py::arg("protein"), py::arg("ligand"), py::arg("complex"), py::arg("epsilon"),
           "Compute K* score with default method (AStar)")
        .def("compute", [](const KStarWorkflow<double>& workflow,
                           const EnergyMatrix<double>& protein,
                           const EnergyMatrix<double>& ligand,
                           const EnergyMatrix<double>& complex,
                           double epsilon,
                           PartitionFunctionMethod method) {
            return workflow.compute(protein, ligand, complex, epsilon, method, PartitionFunction<double>::ComputeOptions{});
        }, py::arg("protein"), py::arg("ligand"), py::arg("complex"), py::arg("epsilon"), py::arg("method"),
           "Compute K* score with specified method")
        .def("compute", [](const KStarWorkflow<double>& workflow,
                           const EnergyMatrix<double>& protein,
                           const EnergyMatrix<double>& ligand,
                           const EnergyMatrix<double>& complex,
                           double epsilon,
                           PartitionFunctionMethod method,
                           PartitionFunction<double>::ComputeOptions options) {
            return workflow.compute(protein, ligand, complex, epsilon, method, options);
        }, py::arg("protein"), py::arg("ligand"), py::arg("complex"), py::arg("epsilon"), py::arg("method"), py::arg("options"),
           "Compute K* score with method and options");

    // EnergyMatrixLoader - free function for convenience
    m.def("load_energy_matrix", &EnergyMatrixLoader<double>::loadFromFile,
          py::arg("filepath"),
          "Load EnergyMatrix from binary file exported by Java (.emat.bin format)");

    // Trace helper: returns (PartitionFunctionResult, [PartitionFunctionTraceStep...])
    m.def(
        "trace_partition_function",
        [](const EnergyMatrix<double>& emat,
           double epsilon,
           AStarVariant astar_variant,
           int64_t max_steps,
           bool capture_assignments) {
            PartitionFunction<double> pfunc;
            PartitionFunction<double>::ComputeOptions opts;
            opts.allow_exact_enumeration = false;
            opts.astar_variant = astar_variant;
            opts.trace_max_steps = max_steps;
            opts.trace_capture_assignments = capture_assignments;
            std::vector<PartitionFunctionTraceStep<double>> trace;
            opts.trace_steps = &trace;

            auto r = pfunc.compute(emat, epsilon, PartitionFunctionMethod::AStar, opts);
            return py::make_tuple(r, trace);
        },
        py::arg("energy_matrix"),
        py::arg("epsilon"),
        py::arg("astar_variant") = AStarVariant::Fast,
        py::arg("max_steps") = int64_t(200),
        py::arg("capture_assignments") = false,
        "Run A* partition function and capture step-by-step trace.\n\n"
        "Returns: (PartitionFunctionResult, List[PartitionFunctionTraceStep])."
    );
}

PYBIND11_MODULE(kstar_cpp, m) {
    m.doc() = "Python bindings for OSPREY C++ K* implementation";

    bind_kstar_double(m);
}
