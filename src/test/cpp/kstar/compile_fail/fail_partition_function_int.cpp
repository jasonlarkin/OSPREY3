// Intentionally invalid: PartitionFunction<T> requires std::floating_point<T>.

#include "partition_function.hpp"

void must_fail_concept() {
    osprey::kstar::PartitionFunction<int> p;
    (void)p;
}

