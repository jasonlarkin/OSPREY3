#ifndef OSPREY_KSTAR_PARTITION_FUNCTION_MPFR_HPP
#define OSPREY_KSTAR_PARTITION_FUNCTION_MPFR_HPP

/**
 * MPFR-based partition function implementation (optional alternative to log-space).
 * 
 * Uses MPFR (Multiple Precision Floating-Point Reliable) library for arbitrary
 * precision arithmetic, similar to OSPREY's BigDecimal approach.
 * 
 * This provides exact matching with OSPREY's results but with performance overhead.
 * Use log-space implementation for performance, MPFR for exact validation.
 */

#ifdef OSPREY_USE_MPFR

#include <mpfr.h>
#include <cstdint>
#include "partition_function.hpp"

namespace osprey {
namespace kstar {

/**
 * MPFR-based partition function calculator.
 * 
 * Uses MPFR for arbitrary precision (64 decimal digits, matching OSPREY's BigDecimal precision).
 */
template<std::floating_point T>
class PartitionFunctionMPFR {
public:
    PartitionFunctionMPFR();
    ~PartitionFunctionMPFR();
    
    /**
     * Compute partition function using MPFR (arbitrary precision).
     * 
     * This matches OSPREY's BigDecimal-based computation exactly.
     */
    [[nodiscard]] PartitionFunctionResult<T> compute(
        const EnergyMatrix<T>& emat,
        T epsilon
    );
    
private:
    static constexpr int PRECISION_BITS = 213;  // ~64 decimal digits (213 bits ≈ 64 * log2(10))
    mpfr_t qstar;      // MPFR variable for q*
    mpfr_t qprime;     // MPFR variable for q'
    mpfr_t pstar;      // MPFR variable for p*
    mpfr_t RT;         // RT constant in MPFR
    mpfr_t tmp;        // Temporary MPFR variable
    
    /**
     * Compute Boltzmann weight: exp(-E/RT) using MPFR.
     */
    void boltzmannWeight(mpfr_t result, T energy);
    
    /**
     * Compute effective epsilon: (qprime + pstar) / (qstar + qprime + pstar)
     */
    T computeEffectiveEpsilon();
};

} // namespace kstar
} // namespace osprey

#endif // OSPREY_USE_MPFR

#endif // OSPREY_KSTAR_PARTITION_FUNCTION_MPFR_HPP

