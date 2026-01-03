#include "kstar_workflow.hpp"

namespace osprey::kstar {

template<std::floating_point T>
[[nodiscard]] KStarWorkflowResult<T> KStarWorkflow<T>::compute(
    const EnergyMatrix<T>& protein,
    const EnergyMatrix<T>& ligand,
    const EnergyMatrix<T>& complex,
    T epsilon,
    PartitionFunctionMethod method,
    typename PartitionFunction<T>::ComputeOptions pfunc_options
) const {
    PartitionFunction<T> pfunc;

    KStarWorkflowResult<T> out;
    out.pfuncs.protein = pfunc.compute(protein, epsilon, method, pfunc_options);
    out.pfuncs.ligand = pfunc.compute(ligand, epsilon, method, pfunc_options);
    out.pfuncs.complex = pfunc.compute(complex, epsilon, method, pfunc_options);

    // log10(K*) = log10(Qc) - log10(Qp) - log10(Ql)
    out.log10_lower_bound =
        out.pfuncs.complex.lower_bound - out.pfuncs.protein.upper_bound - out.pfuncs.ligand.upper_bound;
    out.log10_upper_bound =
        out.pfuncs.complex.upper_bound - out.pfuncs.protein.lower_bound - out.pfuncs.ligand.lower_bound;
    out.log10_value = (out.log10_lower_bound + out.log10_upper_bound) / T(2);

    out.converged = out.pfuncs.protein.converged && out.pfuncs.ligand.converged && out.pfuncs.complex.converged;
    return out;
}

template class KStarWorkflow<double>;
template class KStarWorkflow<float>;

} // namespace osprey::kstar

