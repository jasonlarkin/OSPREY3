#ifndef CONFECALC_ENERGY_AMBEREEF1_SIMD_H
#define CONFECALC_ENERGY_AMBEREEF1_SIMD_H

// This file is included at the end of energy_ambereef1.h
// All types are already defined, we just need these headers
#include "array.h"
#include "real3.h"
#include <immintrin.h>  // AVX2/AVX-512 intrinsics
#include <cassert>
#include <cmath>

namespace osprey::ambereef1 {

#ifdef USE_AVX512
	// AVX-512 version: process 8 atom pairs simultaneously
	static inline double calc_simd(const Array<Real3<double>> & atoms, const Params & params, const AtomPairs & pairs) {
		double energy = 0.0;

		// Process amber interactions with SIMD
		auto pair_amber = reinterpret_cast<const AtomPairAmber<double> *>(&pairs + 1);
		int num_amber = pairs.num_amber;
		
		// SIMD loop: process 8 pairs at a time (AVX-512)
		int i = 0;
		int64_t atoms_size = atoms.get_size();
		for (; i + 7 < num_amber; i += 8) {
			// Load 8 atom1 indices
			int idx1[8] = {
				pair_amber[i+0].atomi1, pair_amber[i+1].atomi1, pair_amber[i+2].atomi1, pair_amber[i+3].atomi1,
				pair_amber[i+4].atomi1, pair_amber[i+5].atomi1, pair_amber[i+6].atomi1, pair_amber[i+7].atomi1
			};
			
			// Load 8 atom2 indices
			int idx2[8] = {
				pair_amber[i+0].atomi2, pair_amber[i+1].atomi2, pair_amber[i+2].atomi2, pair_amber[i+3].atomi2,
				pair_amber[i+4].atomi2, pair_amber[i+5].atomi2, pair_amber[i+6].atomi2, pair_amber[i+7].atomi2
			};
			
			// Validate indices are within bounds
			for (int j = 0; j < 8; j++) {
				assert(idx1[j] >= 0 && idx1[j] < atoms_size);
				assert(idx2[j] >= 0 && idx2[j] < atoms_size);
			}
			
			// Load coordinates for 8 atom1s (AVX-512: 8 doubles)
			__m512d x1 = _mm512_set_pd(atoms[idx1[7]].x, atoms[idx1[6]].x, atoms[idx1[5]].x, atoms[idx1[4]].x,
			                           atoms[idx1[3]].x, atoms[idx1[2]].x, atoms[idx1[1]].x, atoms[idx1[0]].x);
			__m512d y1 = _mm512_set_pd(atoms[idx1[7]].y, atoms[idx1[6]].y, atoms[idx1[5]].y, atoms[idx1[4]].y,
			                           atoms[idx1[3]].y, atoms[idx1[2]].y, atoms[idx1[1]].y, atoms[idx1[0]].y);
			__m512d z1 = _mm512_set_pd(atoms[idx1[7]].z, atoms[idx1[6]].z, atoms[idx1[5]].z, atoms[idx1[4]].z,
			                           atoms[idx1[3]].z, atoms[idx1[2]].z, atoms[idx1[1]].z, atoms[idx1[0]].z);
			
			// Load coordinates for 8 atom2s
			__m512d x2 = _mm512_set_pd(atoms[idx2[7]].x, atoms[idx2[6]].x, atoms[idx2[5]].x, atoms[idx2[4]].x,
			                           atoms[idx2[3]].x, atoms[idx2[2]].x, atoms[idx2[1]].x, atoms[idx2[0]].x);
			__m512d y2 = _mm512_set_pd(atoms[idx2[7]].y, atoms[idx2[6]].y, atoms[idx2[5]].y, atoms[idx2[4]].y,
			                           atoms[idx2[3]].y, atoms[idx2[2]].y, atoms[idx2[1]].y, atoms[idx2[0]].y);
			__m512d z2 = _mm512_set_pd(atoms[idx2[7]].z, atoms[idx2[6]].z, atoms[idx2[5]].z, atoms[idx2[4]].z,
			                           atoms[idx2[3]].z, atoms[idx2[2]].z, atoms[idx2[1]].z, atoms[idx2[0]].z);
			
			// Calculate dx, dy, dz for 8 pairs
			__m512d dx = _mm512_sub_pd(x1, x2);
			__m512d dy = _mm512_sub_pd(y1, y2);
			__m512d dz = _mm512_sub_pd(z1, z2);
			
			// Calculate r2 = dx*dx + dy*dy + dz*dz for 8 pairs
			__m512d dx2 = _mm512_mul_pd(dx, dx);
			__m512d dy2 = _mm512_mul_pd(dy, dy);
			__m512d dz2 = _mm512_mul_pd(dz, dz);
			__m512d r2_vec = _mm512_add_pd(_mm512_add_pd(dx2, dy2), dz2);
			
			// Extract r2 values
			alignas(64) double r2_array[8];
			_mm512_store_pd(r2_array, r2_vec);
			
			// Calculate r = sqrt(r2) and energy for each pair
			for (int j = 0; j < 8; j++) {
				double r2 = r2_array[j];
				assert(!std::isnan(r2));
				double r = std::sqrt(r2);
				const AtomPairAmber<double>& pair = pair_amber[i+j];
				energy += pair.calc(r, r2, params.distance_dependent_dielectric);
			}
		}
		
		// Handle remaining pairs (scalar)
		for (; i < num_amber; i++) {
			Real3<double> atom1 = atoms[pair_amber[i].atomi1];
			Real3<double> atom2 = atoms[pair_amber[i].atomi2];
			double r2 = distance_sq(atom1, atom2);
			assert(!std::isnan(r2));
			double r = std::sqrt(r2);
			energy += pair_amber[i].calc(r, r2, params.distance_dependent_dielectric);
		}

		// Process eef1 interactions with SIMD (same pattern)
		auto pair_eef1 = reinterpret_cast<const AtomPairEef1<double> *>(pair_amber + num_amber);
		int num_eef1 = pairs.num_eef1;
		
		i = 0;
		for (; i + 7 < num_eef1; i += 8) {
			// Load 8 atom1 indices
			int idx1[8] = {
				pair_eef1[i+0].atomi1, pair_eef1[i+1].atomi1, pair_eef1[i+2].atomi1, pair_eef1[i+3].atomi1,
				pair_eef1[i+4].atomi1, pair_eef1[i+5].atomi1, pair_eef1[i+6].atomi1, pair_eef1[i+7].atomi1
			};
			
			// Load 8 atom2 indices
			int idx2[8] = {
				pair_eef1[i+0].atomi2, pair_eef1[i+1].atomi2, pair_eef1[i+2].atomi2, pair_eef1[i+3].atomi2,
				pair_eef1[i+4].atomi2, pair_eef1[i+5].atomi2, pair_eef1[i+6].atomi2, pair_eef1[i+7].atomi2
			};
			
			// Validate indices
			for (int j = 0; j < 8; j++) {
				assert(idx1[j] >= 0 && idx1[j] < atoms_size);
				assert(idx2[j] >= 0 && idx2[j] < atoms_size);
			}
			
			// Load coordinates for 8 atom1s
			__m512d x1 = _mm512_set_pd(atoms[idx1[7]].x, atoms[idx1[6]].x, atoms[idx1[5]].x, atoms[idx1[4]].x,
			                           atoms[idx1[3]].x, atoms[idx1[2]].x, atoms[idx1[1]].x, atoms[idx1[0]].x);
			__m512d y1 = _mm512_set_pd(atoms[idx1[7]].y, atoms[idx1[6]].y, atoms[idx1[5]].y, atoms[idx1[4]].y,
			                           atoms[idx1[3]].y, atoms[idx1[2]].y, atoms[idx1[1]].y, atoms[idx1[0]].y);
			__m512d z1 = _mm512_set_pd(atoms[idx1[7]].z, atoms[idx1[6]].z, atoms[idx1[5]].z, atoms[idx1[4]].z,
			                           atoms[idx1[3]].z, atoms[idx1[2]].z, atoms[idx1[1]].z, atoms[idx1[0]].z);
			
			// Load coordinates for 8 atom2s
			__m512d x2 = _mm512_set_pd(atoms[idx2[7]].x, atoms[idx2[6]].x, atoms[idx2[5]].x, atoms[idx2[4]].x,
			                           atoms[idx2[3]].x, atoms[idx2[2]].x, atoms[idx2[1]].x, atoms[idx2[0]].x);
			__m512d y2 = _mm512_set_pd(atoms[idx2[7]].y, atoms[idx2[6]].y, atoms[idx2[5]].y, atoms[idx2[4]].y,
			                           atoms[idx2[3]].y, atoms[idx2[2]].y, atoms[idx2[1]].y, atoms[idx2[0]].y);
			__m512d z2 = _mm512_set_pd(atoms[idx2[7]].z, atoms[idx2[6]].z, atoms[idx2[5]].z, atoms[idx2[4]].z,
			                           atoms[idx2[3]].z, atoms[idx2[2]].z, atoms[idx2[1]].z, atoms[idx2[0]].z);
			
			// Calculate dx, dy, dz for 8 pairs
			__m512d dx = _mm512_sub_pd(x1, x2);
			__m512d dy = _mm512_sub_pd(y1, y2);
			__m512d dz = _mm512_sub_pd(z1, z2);
			
			// Calculate r2 = dx*dx + dy*dy + dz*dz for 8 pairs
			__m512d dx2 = _mm512_mul_pd(dx, dx);
			__m512d dy2 = _mm512_mul_pd(dy, dy);
			__m512d dz2 = _mm512_mul_pd(dz, dz);
			__m512d r2_vec = _mm512_add_pd(_mm512_add_pd(dx2, dy2), dz2);
			
			// Extract r2 values
			alignas(64) double r2_array[8];
			_mm512_store_pd(r2_array, r2_vec);
			
			// Calculate r = sqrt(r2) and energy for each pair
			for (int j = 0; j < 8; j++) {
				double r2 = r2_array[j];
				assert(!std::isnan(r2));
				double r = std::sqrt(r2);
				const AtomPairEef1<double>& pair = pair_eef1[i+j];
				energy += pair.calc(r, r2);
			}
		}
		
		// Handle remaining pairs (scalar)
		for (; i < num_eef1; i++) {
			Real3<double> atom1 = atoms[pair_eef1[i].atomi1];
			Real3<double> atom2 = atoms[pair_eef1[i].atomi2];
			double r2 = distance_sq(atom1, atom2);
			assert(!std::isnan(r2));
			double r = std::sqrt(r2);
			energy += pair_eef1[i].calc(r, r2);
		}

		return energy;
	}
#else
	// AVX2 version: process 4 atom pairs simultaneously
	static inline double calc_simd(const Array<Real3<double>> & atoms, const Params & params, const AtomPairs & pairs) {

		double energy = 0.0;

		// Process amber interactions with SIMD
		auto pair_amber = reinterpret_cast<const AtomPairAmber<double> *>(&pairs + 1);
		int num_amber = pairs.num_amber;
		
		// SIMD loop: process 4 pairs at a time
		int i = 0;
		int64_t atoms_size = atoms.get_size();
		for (; i + 3 < num_amber; i += 4) {
			// Load 4 atom1 indices
			int idx1[4] = {
				pair_amber[i+0].atomi1,
				pair_amber[i+1].atomi1,
				pair_amber[i+2].atomi1,
				pair_amber[i+3].atomi1
			};
			
			// Load 4 atom2 indices
			int idx2[4] = {
				pair_amber[i+0].atomi2,
				pair_amber[i+1].atomi2,
				pair_amber[i+2].atomi2,
				pair_amber[i+3].atomi2
			};
			
			// Validate indices are within bounds
			for (int j = 0; j < 4; j++) {
				assert(idx1[j] >= 0 && idx1[j] < atoms_size);
				assert(idx2[j] >= 0 && idx2[j] < atoms_size);
			}
			
			// Load coordinates for 4 atom1s
			__m256d x1 = _mm256_set_pd(atoms[idx1[3]].x, atoms[idx1[2]].x, atoms[idx1[1]].x, atoms[idx1[0]].x);
			__m256d y1 = _mm256_set_pd(atoms[idx1[3]].y, atoms[idx1[2]].y, atoms[idx1[1]].y, atoms[idx1[0]].y);
			__m256d z1 = _mm256_set_pd(atoms[idx1[3]].z, atoms[idx1[2]].z, atoms[idx1[1]].z, atoms[idx1[0]].z);
			
			// Load coordinates for 4 atom2s
			__m256d x2 = _mm256_set_pd(atoms[idx2[3]].x, atoms[idx2[2]].x, atoms[idx2[1]].x, atoms[idx2[0]].x);
			__m256d y2 = _mm256_set_pd(atoms[idx2[3]].y, atoms[idx2[2]].y, atoms[idx2[1]].y, atoms[idx2[0]].y);
			__m256d z2 = _mm256_set_pd(atoms[idx2[3]].z, atoms[idx2[2]].z, atoms[idx2[1]].z, atoms[idx2[0]].z);
			
			// Calculate dx, dy, dz for 4 pairs
			__m256d dx = _mm256_sub_pd(x1, x2);
			__m256d dy = _mm256_sub_pd(y1, y2);
			__m256d dz = _mm256_sub_pd(z1, z2);
			
			// Calculate r2 = dx*dx + dy*dy + dz*dz for 4 pairs
			__m256d dx2 = _mm256_mul_pd(dx, dx);
			__m256d dy2 = _mm256_mul_pd(dy, dy);
			__m256d dz2 = _mm256_mul_pd(dz, dz);
			__m256d r2_vec = _mm256_add_pd(_mm256_add_pd(dx2, dy2), dz2);
			
			// Extract r2 values
			alignas(32) double r2_array[4];
			_mm256_store_pd(r2_array, r2_vec);
			
			// Calculate r = sqrt(r2) and energy for each pair
			for (int j = 0; j < 4; j++) {
				double r2 = r2_array[j];
				assert(!std::isnan(r2));
				double r = std::sqrt(r2);
				// Call calc method on AtomPairAmber
				const AtomPairAmber<double>& pair = pair_amber[i+j];
				energy += pair.calc(r, r2, params.distance_dependent_dielectric);
			}
		}
		
		// Handle remaining pairs (scalar)
		for (; i < num_amber; i++) {
			Real3<double> atom1 = atoms[pair_amber[i].atomi1];
			Real3<double> atom2 = atoms[pair_amber[i].atomi2];
			double r2 = distance_sq(atom1, atom2);
			assert(!std::isnan(r2));
			double r = std::sqrt(r2);
			energy += pair_amber[i].calc(r, r2, params.distance_dependent_dielectric);
		}

		// Process eef1 interactions with SIMD (same pattern)
		// Advance pointer past all Amber pairs to point to first EEF1 pair
		auto pair_eef1 = reinterpret_cast<const AtomPairEef1<double> *>(pair_amber + num_amber);
		int num_eef1 = pairs.num_eef1;
		
		i = 0;
		for (; i + 3 < num_eef1; i += 4) {
			// Load 4 atom1 indices
			int idx1[4] = {
				pair_eef1[i+0].atomi1,
				pair_eef1[i+1].atomi1,
				pair_eef1[i+2].atomi1,
				pair_eef1[i+3].atomi1
			};
			
			// Load 4 atom2 indices
			int idx2[4] = {
				pair_eef1[i+0].atomi2,
				pair_eef1[i+1].atomi2,
				pair_eef1[i+2].atomi2,
				pair_eef1[i+3].atomi2
			};
			
			// Validate indices are within bounds
			for (int j = 0; j < 4; j++) {
				assert(idx1[j] >= 0 && idx1[j] < atoms_size);
				assert(idx2[j] >= 0 && idx2[j] < atoms_size);
			}
			
			// Load coordinates for 4 atom1s
			__m256d x1 = _mm256_set_pd(atoms[idx1[3]].x, atoms[idx1[2]].x, atoms[idx1[1]].x, atoms[idx1[0]].x);
			__m256d y1 = _mm256_set_pd(atoms[idx1[3]].y, atoms[idx1[2]].y, atoms[idx1[1]].y, atoms[idx1[0]].y);
			__m256d z1 = _mm256_set_pd(atoms[idx1[3]].z, atoms[idx1[2]].z, atoms[idx1[1]].z, atoms[idx1[0]].z);
			
			// Load coordinates for 4 atom2s
			__m256d x2 = _mm256_set_pd(atoms[idx2[3]].x, atoms[idx2[2]].x, atoms[idx2[1]].x, atoms[idx2[0]].x);
			__m256d y2 = _mm256_set_pd(atoms[idx2[3]].y, atoms[idx2[2]].y, atoms[idx2[1]].y, atoms[idx2[0]].y);
			__m256d z2 = _mm256_set_pd(atoms[idx2[3]].z, atoms[idx2[2]].z, atoms[idx2[1]].z, atoms[idx2[0]].z);
			
			// Calculate dx, dy, dz for 4 pairs
			__m256d dx = _mm256_sub_pd(x1, x2);
			__m256d dy = _mm256_sub_pd(y1, y2);
			__m256d dz = _mm256_sub_pd(z1, z2);
			
			// Calculate r2 = dx*dx + dy*dy + dz*dz for 4 pairs
			__m256d dx2 = _mm256_mul_pd(dx, dx);
			__m256d dy2 = _mm256_mul_pd(dy, dy);
			__m256d dz2 = _mm256_mul_pd(dz, dz);
			__m256d r2_vec = _mm256_add_pd(_mm256_add_pd(dx2, dy2), dz2);
			
			// Extract r2 values
			alignas(32) double r2_array[4];
			_mm256_store_pd(r2_array, r2_vec);
			
			// Calculate r = sqrt(r2) and energy for each pair
			for (int j = 0; j < 4; j++) {
				double r2 = r2_array[j];
				assert(!std::isnan(r2));
				double r = std::sqrt(r2);
				// Call calc method on AtomPairEef1
				const AtomPairEef1<double>& pair = pair_eef1[i+j];
				energy += pair.calc(r, r2);
			}
		}
		
		// Handle remaining pairs (scalar)
		for (; i < num_eef1; i++) {
			Real3<double> atom1 = atoms[pair_eef1[i].atomi1];
			Real3<double> atom2 = atoms[pair_eef1[i].atomi2];
			double r2 = distance_sq(atom1, atom2);
			assert(!std::isnan(r2));
			double r = std::sqrt(r2);
			energy += pair_eef1[i].calc(r, r2);
		}

		return energy;
	}

}

#endif //CONFECALC_ENERGY_AMBEREEF1_SIMD_H

