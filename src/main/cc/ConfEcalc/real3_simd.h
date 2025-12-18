#ifndef CONFECALC_REAL3_SIMD_H
#define CONFECALC_REAL3_SIMD_H

// Include immintrin.h in global namespace before any other includes
#include <immintrin.h>

#include "real3.h"

namespace osprey {

// SIMD-optimized Real3 operations
// Process multiple Real3 vectors simultaneously

#ifdef USE_SIMD

// Intrinsics are included in global namespace via <immintrin.h> at top of file
// Use unqualified names - they'll be found in global namespace

// AVX2: Process 4 Real3<double> vectors simultaneously
inline __attribute__((target("avx2,fma")))
void normalize_avx2(Real3<double>* vecs, int count) {
    const Real3<double>* vecs_end = vecs + count;
    int i = 0;
    
    // Process 4 vectors at a time
    for (; i + 3 < count; i += 4) {
        // Load 4 vectors (x, y, z components)
::__m256d x = ::_mm256_set_pd(vecs[i+3].x, vecs[i+2].x, vecs[i+1].x, vecs[i+0].x);
::__m256d y = ::_mm256_set_pd(vecs[i+3].y, vecs[i+2].y, vecs[i+1].y, vecs[i+0].y);
::__m256d z = ::_mm256_set_pd(vecs[i+3].z, vecs[i+2].z, vecs[i+1].z, vecs[i+0].z);
        
        // Calculate length squared: x*x + y*y + z*z
::__m256d len_sq = ::_mm256_fmadd_pd(z, z, ::_mm256_fmadd_pd(y, y, ::_mm256_mul_pd(x, x)));
        
        // Calculate length and reciprocal
::__m256d len = ::_mm256_sqrt_pd(len_sq);
::__m256d inv_len = ::_mm256_div_pd(::_mm256_set1_pd(1.0), len);
        
        // Normalize: x *= inv_len, y *= inv_len, z *= inv_len
        x = ::_mm256_mul_pd(x, inv_len);
        y = ::_mm256_mul_pd(y, inv_len);
        z = ::_mm256_mul_pd(z, inv_len);
        
        // Store results
        alignas(32) double x_arr[4], y_arr[4], z_arr[4];
::_mm256_store_pd(x_arr, x);
::_mm256_store_pd(y_arr, y);
::_mm256_store_pd(z_arr, z);
        
        vecs[i+0].x = x_arr[0]; vecs[i+0].y = y_arr[0]; vecs[i+0].z = z_arr[0];
        vecs[i+1].x = x_arr[1]; vecs[i+1].y = y_arr[1]; vecs[i+1].z = z_arr[1];
        vecs[i+2].x = x_arr[2]; vecs[i+2].y = y_arr[2]; vecs[i+2].z = z_arr[2];
        vecs[i+3].x = x_arr[3]; vecs[i+3].y = y_arr[3]; vecs[i+3].z = z_arr[3];
    }
    
    // Handle remainder (scalar)
    for (; i < count; i++) {
        vecs[i].normalize();
    }
}

// AVX2: Vectorized dot product for 4 vectors
inline __attribute__((target("avx2,fma")))
void dot_avx2(const Real3<double>* a, const Real3<double>* b, double* results, int count) {
    int i = 0;
    
    // Process 4 vectors at a time
    for (; i + 3 < count; i += 4) {
        // Load a vectors
::__m256d ax = ::_mm256_set_pd(a[i+3].x, a[i+2].x, a[i+1].x, a[i+0].x);
::__m256d ay = ::_mm256_set_pd(a[i+3].y, a[i+2].y, a[i+1].y, a[i+0].y);
::__m256d az = ::_mm256_set_pd(a[i+3].z, a[i+2].z, a[i+1].z, a[i+0].z);
        
        // Load b vectors
::__m256d bx = ::_mm256_set_pd(b[i+3].x, b[i+2].x, b[i+1].x, b[i+0].x);
::__m256d by = ::_mm256_set_pd(b[i+3].y, b[i+2].y, b[i+1].y, b[i+0].y);
::__m256d bz = ::_mm256_set_pd(b[i+3].z, b[i+2].z, b[i+1].z, b[i+0].z);
        
        // Calculate dot product: ax*bx + ay*by + az*bz
::__m256d dot = ::_mm256_fmadd_pd(az, bz, ::_mm256_fmadd_pd(ay, by, ::_mm256_mul_pd(ax, bx)));
        
        // Store results
        alignas(32) double dot_arr[4];
::_mm256_store_pd(dot_arr, dot);
        results[i+0] = dot_arr[0];
        results[i+1] = dot_arr[1];
        results[i+2] = dot_arr[2];
        results[i+3] = dot_arr[3];
    }
    
    // Handle remainder (scalar)
    for (; i < count; i++) {
        results[i] = a[i].dot(b[i]);
    }
}

// AVX2: Vectorized cross product for 4 vectors
inline __attribute__((target("avx2,fma")))
void cross_avx2(const Real3<double>* a, const Real3<double>* b, Real3<double>* results, int count) {
    int i = 0;
    
    // Process 4 vectors at a time
    for (; i + 3 < count; i += 4) {
        // Load a vectors
::__m256d ax = ::_mm256_set_pd(a[i+3].x, a[i+2].x, a[i+1].x, a[i+0].x);
::__m256d ay = ::_mm256_set_pd(a[i+3].y, a[i+2].y, a[i+1].y, a[i+0].y);
::__m256d az = ::_mm256_set_pd(a[i+3].z, a[i+2].z, a[i+1].z, a[i+0].z);
        
        // Load b vectors
::__m256d bx = ::_mm256_set_pd(b[i+3].x, b[i+2].x, b[i+1].x, b[i+0].x);
::__m256d by = ::_mm256_set_pd(b[i+3].y, b[i+2].y, b[i+1].y, b[i+0].y);
::__m256d bz = ::_mm256_set_pd(b[i+3].z, b[i+2].z, b[i+1].z, b[i+0].z);
        
        // Cross product: (ay*bz - az*by, az*bx - ax*bz, ax*by - ay*bx)
::__m256d rx = ::_mm256_fmsub_pd(ay, bz, ::_mm256_mul_pd(az, by));
::__m256d ry = ::_mm256_fmsub_pd(az, bx, ::_mm256_mul_pd(ax, bz));
::__m256d rz = ::_mm256_fmsub_pd(ax, by, ::_mm256_mul_pd(ay, bx));
        
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
        results[i] = cross(a[i], b[i]);
    }
}

// AVX2: Vectorized vector addition for 4 vectors
inline __attribute__((target("avx2,fma")))
void add_avx2(const Real3<double>* a, const Real3<double>* b, Real3<double>* results, int count) {
    int i = 0;
    
    // Process 4 vectors at a time
    for (; i + 3 < count; i += 4) {
        // Load a vectors
::__m256d ax = ::_mm256_set_pd(a[i+3].x, a[i+2].x, a[i+1].x, a[i+0].x);
::__m256d ay = ::_mm256_set_pd(a[i+3].y, a[i+2].y, a[i+1].y, a[i+0].y);
::__m256d az = ::_mm256_set_pd(a[i+3].z, a[i+2].z, a[i+1].z, a[i+0].z);
        
        // Load b vectors
::__m256d bx = ::_mm256_set_pd(b[i+3].x, b[i+2].x, b[i+1].x, b[i+0].x);
::__m256d by = ::_mm256_set_pd(b[i+3].y, b[i+2].y, b[i+1].y, b[i+0].y);
::__m256d bz = ::_mm256_set_pd(b[i+3].z, b[i+2].z, b[i+1].z, b[i+0].z);
        
        // Add: a + b
::__m256d rx = ::_mm256_add_pd(ax, bx);
::__m256d ry = ::_mm256_add_pd(ay, by);
::__m256d rz = ::_mm256_add_pd(az, bz);
        
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
        results[i] = a[i] + b[i];
    }
}

// AVX2: Vectorized vector subtraction for 4 vectors
inline __attribute__((target("avx2,fma")))
void subtract_avx2(const Real3<double>* a, const Real3<double>* b, Real3<double>* results, int count) {
    int i = 0;
    
    // Process 4 vectors at a time
    for (; i + 3 < count; i += 4) {
        // Load a vectors
::__m256d ax = ::_mm256_set_pd(a[i+3].x, a[i+2].x, a[i+1].x, a[i+0].x);
::__m256d ay = ::_mm256_set_pd(a[i+3].y, a[i+2].y, a[i+1].y, a[i+0].y);
::__m256d az = ::_mm256_set_pd(a[i+3].z, a[i+2].z, a[i+1].z, a[i+0].z);
        
        // Load b vectors
::__m256d bx = ::_mm256_set_pd(b[i+3].x, b[i+2].x, b[i+1].x, b[i+0].x);
::__m256d by = ::_mm256_set_pd(b[i+3].y, b[i+2].y, b[i+1].y, b[i+0].y);
::__m256d bz = ::_mm256_set_pd(b[i+3].z, b[i+2].z, b[i+1].z, b[i+0].z);
        
        // Subtract: a - b
::__m256d rx = ::_mm256_sub_pd(ax, bx);
::__m256d ry = ::_mm256_sub_pd(ay, by);
::__m256d rz = ::_mm256_sub_pd(az, bz);
        
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
        results[i] = a[i] - b[i];
    }
}

#ifdef USE_AVX512
// AVX-512: Process 8 Real3<double> vectors simultaneously
// Note: AVX-512 support may be limited due to namespace conflicts
// Disabled for now - can be enabled when namespace pollution is resolved
#endif // USE_AVX512

#endif // USE_SIMD

} // namespace osprey

#endif // CONFECALC_REAL3_SIMD_H

