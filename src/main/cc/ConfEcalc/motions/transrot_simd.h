#ifndef CONFECALC_MOTIONS_TRANSROT_SIMD_H
#define CONFECALC_MOTIONS_TRANSROT_SIMD_H

#include <concepts>
#include <type_traits>

#include "../rotation_simd.h"
#include "../real3_simd.h"
#include <immintrin.h>

namespace osprey::motions {

template<std::floating_point T>
struct TranslationRotation;

#ifdef USE_SIMD

// AVX2: SIMD-optimized apply() for TranslationRotation
// Processes multiple atoms simultaneously
template<std::floating_point T>
inline __attribute__((target("avx2,fma")))
void apply_transrot_avx2_impl(typename TranslationRotation<T>::TransRotDofs& transrot) {
    if constexpr (!std::is_same_v<T, double>) {
        return; // Only for double precision
    }
    
    using namespace osprey;
    
    auto& desc = transrot.desc;
    auto& assignment = transrot.assignment;
    auto& modified_atomi = transrot.modified_atomi;
    
    // Invert current transform
    transrot.transform_current.translation.negate();
    transrot.transform_current.rotation.invert();
    
    // Build next transform
    typename TranslationRotation<T>::Transform transform_next;
    transform_next.translation = { transrot.dof_x.get(), transrot.dof_y.get(), transrot.dof_z.get() };
    transform_next.rotation.set_xyz(transrot.dof_psi.get(), transrot.dof_theta.get(), transrot.dof_phi.get());
    
    Real3<double>* atoms_ptr = const_cast<Real3<double>*>(assignment.atoms.pointer());
    Real3<double> centroid = desc.centroid;
    
    int count = modified_atomi.get_size();
    int i = 0;
    
    // Process 4 atoms at a time with AVX2
    for (; i + 3 < count; i += 4) {
        // Gather atom indices
        int64_t idx[4] = {
            modified_atomi[i+0],
            modified_atomi[i+1],
            modified_atomi[i+2],
            modified_atomi[i+3]
        };
        
        // Load 4 atoms
        Real3<double> atoms[4] = {
            atoms_ptr[idx[0]],
            atoms_ptr[idx[1]],
            atoms_ptr[idx[2]],
            atoms_ptr[idx[3]]
        };
        
        // Subtract centroid (vectorized)
        Real3<double> temp[4];
        for (int j = 0; j < 4; j++) {
            temp[j] = atoms[j] - centroid;
        }
        
        // Undo current transformation: rot_current * (p + trans_current)
        Real3<double> temp2[4];
        Real3<double> trans_current = transrot.transform_current.translation;
        for (int j = 0; j < 4; j++) {
            temp2[j] = temp[j] + trans_current;
        }
        rotate_vectors_avx2(transrot.transform_current.rotation, temp2, temp, 4);
        
        // Apply next transformation: rot_next * p + trans_next
        Real3<double> temp3[4];
        rotate_vectors_avx2(transform_next.rotation, temp, temp3, 4);
        Real3<double> trans_next = transform_next.translation;
        for (int j = 0; j < 4; j++) {
            temp[j] = temp3[j] + trans_next;
        }
        
        // Add centroid back
        for (int j = 0; j < 4; j++) {
            atoms[j] = temp[j] + centroid;
        }
        
        // Store results back
        atoms_ptr[idx[0]] = atoms[0];
        atoms_ptr[idx[1]] = atoms[1];
        atoms_ptr[idx[2]] = atoms[2];
        atoms_ptr[idx[3]] = atoms[3];
    }
    
    // Handle remainder (scalar)
    for (; i < count; i++) {
        int64_t atomi = modified_atomi[i];
        Real3<double> & p = assignment.atoms[atomi];
        
        p -= centroid;
        p = transrot.transform_current.rotation*(p + transrot.transform_current.translation);
        p = transform_next.rotation*p + transform_next.translation;
        p += centroid;
    }
    
    transrot.transform_current = transform_next;
}

#endif // USE_SIMD

} // namespace osprey::motions

#endif // CONFECALC_MOTIONS_TRANSROT_SIMD_H
