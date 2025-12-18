#ifndef CONFECALC_ROTATION_SIMD_H
#define CONFECALC_ROTATION_SIMD_H

#include "rotation.h"
#include "real3.h"
#include <immintrin.h>

namespace osprey {

// SIMD-optimized rotation operations
// Apply rotation matrix to multiple vectors simultaneously

#ifdef USE_SIMD

// Intrinsics are included in global namespace via <immintrin.h> at top of file
// Use unqualified names - they'll be found in global namespace

// AVX2: Apply rotation to 4 vectors simultaneously
// Rotation * Vector: result = (xaxis.dot(v), yaxis.dot(v), zaxis.dot(v))
inline __attribute__((target("avx2,fma")))
void rotate_vectors_avx2(const Rotation<double>& rot, const Real3<double>* vectors, Real3<double>* results, int count) {
    // Load rotation matrix rows (xaxis, yaxis, zaxis)
    ::__m256d rot_x_x = ::_mm256_set1_pd(rot.xaxis.x);
    ::__m256d rot_x_y = ::_mm256_set1_pd(rot.xaxis.y);
    ::__m256d rot_x_z = ::_mm256_set1_pd(rot.xaxis.z);
    
    ::__m256d rot_y_x = ::_mm256_set1_pd(rot.yaxis.x);
    ::__m256d rot_y_y = ::_mm256_set1_pd(rot.yaxis.y);
    ::__m256d rot_y_z = ::_mm256_set1_pd(rot.yaxis.z);
    
    ::__m256d rot_z_x = ::_mm256_set1_pd(rot.zaxis.x);
    ::__m256d rot_z_y = ::_mm256_set1_pd(rot.zaxis.y);
    ::__m256d rot_z_z = ::_mm256_set1_pd(rot.zaxis.z);
    
    int i = 0;
    
    // Process 4 vectors at a time
    for (; i + 3 < count; i += 4) {
        // Load 4 input vectors
::__m256d vx = ::_mm256_set_pd(vectors[i+3].x, vectors[i+2].x, vectors[i+1].x, vectors[i+0].x);
::__m256d vy = ::_mm256_set_pd(vectors[i+3].y, vectors[i+2].y, vectors[i+1].y, vectors[i+0].y);
::__m256d vz = ::_mm256_set_pd(vectors[i+3].z, vectors[i+2].z, vectors[i+1].z, vectors[i+0].z);
        
        // Matrix-vector multiply: result = rotation * vector
        // result.x = rot.xaxis.dot(v) = rot_x_x*vx + rot_x_y*vy + rot_x_z*vz
        // result.y = rot.yaxis.dot(v) = rot_y_x*vx + rot_y_y*vy + rot_y_z*vz
        // result.z = rot.zaxis.dot(v) = rot_z_x*vx + rot_z_y*vy + rot_z_z*vz
::__m256d rx = ::_mm256_fmadd_pd(rot_x_z, vz, ::_mm256_fmadd_pd(rot_x_y, vy, ::_mm256_mul_pd(rot_x_x, vx)));
::__m256d ry = ::_mm256_fmadd_pd(rot_y_z, vz, ::_mm256_fmadd_pd(rot_y_y, vy, ::_mm256_mul_pd(rot_y_x, vx)));
::__m256d rz = ::_mm256_fmadd_pd(rot_z_z, vz, ::_mm256_fmadd_pd(rot_z_y, vy, ::_mm256_mul_pd(rot_z_x, vx)));
        
        // Store results
        alignas(32) double rx_arr[4], ry_arr[4], rz_arr[4];
::_mm256_store_pd(rx_arr, rx);
::_mm256_store_pd(ry_arr, ry);
::_mm256_store_pd(rz_arr, rz);
        
        results[i+0] = Real3<double>(rx_arr[0], ry_arr[0], rz_arr[0]);
        results[i+1] = Real3<double>(rx_arr[1], ry_arr[1], rz_arr[1]);
        results[i+2] = Real3<double>(rx_arr[2], ry_arr[2], rz_arr[2]);
        results[i+3] = Real3<double>(rx_arr[3], ry_arr[3], rz_arr[3]);
    }
    
    // Handle remainder (scalar)
    for (; i < count; i++) {
        results[i] = rot * vectors[i];
    }
}

// AVX2: Apply rotation + translation to 4 vectors simultaneously
inline __attribute__((target("avx2,fma")))
void transform_vectors_avx2(const Rotation<double>& rot, const Real3<double>& translation,
                              const Real3<double>* vectors, Real3<double>* results, int count) {
    // Load rotation matrix
    ::__m256d rot_x_x = ::_mm256_set1_pd(rot.xaxis.x);
    ::__m256d rot_x_y = ::_mm256_set1_pd(rot.xaxis.y);
    ::__m256d rot_x_z = ::_mm256_set1_pd(rot.xaxis.z);
    
    ::__m256d rot_y_x = ::_mm256_set1_pd(rot.yaxis.x);
    ::__m256d rot_y_y = ::_mm256_set1_pd(rot.yaxis.y);
    ::__m256d rot_y_z = ::_mm256_set1_pd(rot.yaxis.z);
    
    ::__m256d rot_z_x = ::_mm256_set1_pd(rot.zaxis.x);
    ::__m256d rot_z_y = ::_mm256_set1_pd(rot.zaxis.y);
    ::__m256d rot_z_z = ::_mm256_set1_pd(rot.zaxis.z);
    
    // Load translation
    ::__m256d tx = ::_mm256_set1_pd(translation.x);
    ::__m256d ty = ::_mm256_set1_pd(translation.y);
    ::__m256d tz = ::_mm256_set1_pd(translation.z);
    
    int i = 0;
    
    // Process 4 vectors at a time
    for (; i + 3 < count; i += 4) {
        // Load 4 input vectors
::__m256d vx = ::_mm256_set_pd(vectors[i+3].x, vectors[i+2].x, vectors[i+1].x, vectors[i+0].x);
::__m256d vy = ::_mm256_set_pd(vectors[i+3].y, vectors[i+2].y, vectors[i+1].y, vectors[i+0].y);
::__m256d vz = ::_mm256_set_pd(vectors[i+3].z, vectors[i+2].z, vectors[i+1].z, vectors[i+0].z);
        
        // Apply rotation: rot * v
::__m256d rx = ::_mm256_fmadd_pd(rot_x_z, vz, ::_mm256_fmadd_pd(rot_x_y, vy, ::_mm256_mul_pd(rot_x_x, vx)));
::__m256d ry = ::_mm256_fmadd_pd(rot_y_z, vz, ::_mm256_fmadd_pd(rot_y_y, vy, ::_mm256_mul_pd(rot_y_x, vx)));
::__m256d rz = ::_mm256_fmadd_pd(rot_z_z, vz, ::_mm256_fmadd_pd(rot_z_y, vy, ::_mm256_mul_pd(rot_z_x, vx)));
        
        // Add translation: rot*v + translation
        rx = ::_mm256_add_pd(rx, tx);
        ry = ::_mm256_add_pd(ry, ty);
        rz = ::_mm256_add_pd(rz, tz);
        
        // Store results
        alignas(32) double rx_arr[4], ry_arr[4], rz_arr[4];
::_mm256_store_pd(rx_arr, rx);
::_mm256_store_pd(ry_arr, ry);
::_mm256_store_pd(rz_arr, rz);
        
        results[i+0] = Real3<double>(rx_arr[0], ry_arr[0], rz_arr[0]);
        results[i+1] = Real3<double>(rx_arr[1], ry_arr[1], rz_arr[1]);
        results[i+2] = Real3<double>(rx_arr[2], ry_arr[2], rz_arr[2]);
        results[i+3] = Real3<double>(rx_arr[3], ry_arr[3], rz_arr[3]);
    }
    
    // Handle remainder (scalar)
    for (; i < count; i++) {
        results[i] = rot * vectors[i] + translation;
    }
}

#ifdef USE_AVX512
// AVX-512: Apply rotation to 8 vectors simultaneously
// Note: Disabled due to namespace conflicts - can be re-enabled when namespace pollution is resolved
#endif // USE_AVX512
#if 0
// Disabled AVX-512 code:
inline __attribute__((target("avx512f,avx512dq,fma")))
void rotate_vectors_avx512(const Rotation<double>& rot, const Real3<double>* vectors, Real3<double>* results, int count) {
    using ::__m512d;
    using ::_mm512_set1_pd;
    using ::_mm512_set_pd;
    using ::_mm512_store_pd;
    using ::_mm512_mul_pd;
    using ::_mm512_fmadd_pd;
    
    // Load rotation matrix rows
    ::__m512d rot_x_x = ::_mm512_set1_pd(rot.xaxis.x);
    ::__m512d rot_x_y = ::_mm512_set1_pd(rot.xaxis.y);
    ::__m512d rot_x_z = ::_mm512_set1_pd(rot.xaxis.z);
    
    ::__m512d rot_y_x = ::_mm512_set1_pd(rot.yaxis.x);
    ::__m512d rot_y_y = ::_mm512_set1_pd(rot.yaxis.y);
    ::__m512d rot_y_z = ::_mm512_set1_pd(rot.yaxis.z);
    
    ::__m512d rot_z_x = ::_mm512_set1_pd(rot.zaxis.x);
    ::__m512d rot_z_y = ::_mm512_set1_pd(rot.zaxis.y);
    ::__m512d rot_z_z = ::_mm512_set1_pd(rot.zaxis.z);
    
    int i = 0;
    
    // Process 8 vectors at a time
    for (; i + 7 < count; i += 8) {
        // Load 8 input vectors
        ::__m512d vx = ::_mm512_set_pd(vectors[i+7].x, vectors[i+6].x, vectors[i+5].x, vectors[i+4].x,
                                   vectors[i+3].x, vectors[i+2].x, vectors[i+1].x, vectors[i+0].x);
        ::__m512d vy = ::_mm512_set_pd(vectors[i+7].y, vectors[i+6].y, vectors[i+5].y, vectors[i+4].y,
                                   vectors[i+3].y, vectors[i+2].y, vectors[i+1].y, vectors[i+0].y);
        ::__m512d vz = ::_mm512_set_pd(vectors[i+7].z, vectors[i+6].z, vectors[i+5].z, vectors[i+4].z,
                                   vectors[i+3].z, vectors[i+2].z, vectors[i+1].z, vectors[i+0].z);
        
        // Matrix-vector multiply
        ::__m512d rx = ::_mm512_fmadd_pd(rot_x_z, vz, ::_mm512_fmadd_pd(rot_x_y, vy, ::_mm512_mul_pd(rot_x_x, vx)));
        ::__m512d ry = ::_mm512_fmadd_pd(rot_y_z, vz, ::_mm512_fmadd_pd(rot_y_y, vy, ::_mm512_mul_pd(rot_y_x, vx)));
        ::__m512d rz = ::_mm512_fmadd_pd(rot_z_z, vz, ::_mm512_fmadd_pd(rot_z_y, vy, ::_mm512_mul_pd(rot_z_x, vx)));
        
        // Store results
        alignas(64) double rx_arr[8], ry_arr[8], rz_arr[8];
        ::_mm512_store_pd(rx_arr, rx);
        ::_mm512_store_pd(ry_arr, ry);
        ::_mm512_store_pd(rz_arr, rz);
        
        for (int j = 0; j < 8; j++) {
            results[i+j] = Real3<double>(rx_arr[j], ry_arr[j], rz_arr[j]);
        }
    }
    
    // Handle remainder (scalar)
    for (; i < count; i++) {
        results[i] = rot * vectors[i];
    }
}
#endif // USE_AVX512

#endif // USE_SIMD

} // namespace osprey

#endif // CONFECALC_ROTATION_SIMD_H

