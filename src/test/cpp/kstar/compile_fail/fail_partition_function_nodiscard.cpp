// Intentionally invalid (when compiled with -Werror=unused-result):
// PartitionFunction::compute is [[nodiscard]] and its return value must not be ignored.

#include "partition_function.hpp"
#include "energy_matrix.hpp"

void must_fail_nodiscard() {
    std::vector<int32_t> num_confs = {1};
    osprey::kstar::EnergyMatrix<double> emat(1, num_confs);
    osprey::kstar::PartitionFunction<double> p;

    // discard result on purpose
    p.compute(emat, 0.5);
}

