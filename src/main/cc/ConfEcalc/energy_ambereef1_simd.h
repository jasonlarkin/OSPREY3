// This file is included at the end of energy_ambereef1.h inside namespace osprey::ambereef1
// All types are already defined, we just need these headers
// No header guard needed - this is an implementation detail included by energy_ambereef1.h

// Standard library headers must use ::std:: to avoid namespace pollution
#include "array.h"
#include "real3.h"
#include "cpu_detect.h"
#include <cassert>
#include <cmath>

// All std namespace usage must use ::std:: prefix to avoid namespace pollution
// (this file is included inside osprey::ambereef1 namespace)

// This file is included inside namespace osprey::ambereef1, so no namespace declaration needed

// Scalar version - always available
inline double calc_scalar(const Array<Real3<double>> & atoms, const Params & params, const AtomPairs & pairs) {
	double energy = 0.0;

	// Process amber interactions
	auto pair_amber = reinterpret_cast<const AtomPairAmber<double> *>(&pairs + 1);
	for (int i=0; i<pairs.num_amber; i++) {
		Real3<double> atom1 = atoms[pair_amber[i].atomi1];
		Real3<double> atom2 = atoms[pair_amber[i].atomi2];
		double r2 = distance_sq(atom1, atom2);
		assert(!::std::isnan(r2));
		double r = ::std::sqrt(r2);
		energy += pair_amber[i].calc(r, r2, params.distance_dependent_dielectric);
	}

	// Process eef1 interactions
	auto pair_eef1 = reinterpret_cast<const AtomPairEef1<double> *>(pair_amber + pairs.num_amber);
	for (int i=0; i<pairs.num_eef1; i++) {
		Real3<double> atom1 = atoms[pair_eef1[i].atomi1];
		Real3<double> atom2 = atoms[pair_eef1[i].atomi2];
		double r2 = distance_sq(atom1, atom2);
		assert(!::std::isnan(r2));
		double r = ::std::sqrt(r2);
		energy += pair_eef1[i].calc(r, r2);
	}

	return energy;
}

#ifdef USE_SIMD

// AVX2 version: process 4 atom pairs simultaneously
// Use attribute target to allow multiple versions to coexist
inline __attribute__((target("avx2,fma")))
double calc_avx2(const Array<Real3<double>> & atoms, const Params & params, const AtomPairs & pairs) {
	double energy = 0.0;

	// Get raw pointer to avoid bounds checking overhead in hot loop
	const Real3<double>* atoms_ptr = atoms.pointer();
	int64_t atoms_size = atoms.get_size();

	// Process amber interactions with SIMD
	auto pair_amber = reinterpret_cast<const AtomPairAmber<double> *>(&pairs + 1);
	int num_amber = pairs.num_amber;
	
	// SIMD loop: process 4 pairs at a time
	int i = 0;
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
		
		// Validate indices are within bounds (once per iteration, not per access)
		for (int j = 0; j < 4; j++) {
			assert(idx1[j] >= 0 && idx1[j] < atoms_size);
			assert(idx2[j] >= 0 && idx2[j] < atoms_size);
		}
		
		// Load coordinates for 4 atom1s (using direct pointer access, no bounds checking)
		__m256d x1 = _mm256_set_pd(atoms_ptr[idx1[3]].x, atoms_ptr[idx1[2]].x, atoms_ptr[idx1[1]].x, atoms_ptr[idx1[0]].x);
		__m256d y1 = _mm256_set_pd(atoms_ptr[idx1[3]].y, atoms_ptr[idx1[2]].y, atoms_ptr[idx1[1]].y, atoms_ptr[idx1[0]].y);
		__m256d z1 = _mm256_set_pd(atoms_ptr[idx1[3]].z, atoms_ptr[idx1[2]].z, atoms_ptr[idx1[1]].z, atoms_ptr[idx1[0]].z);
		
		// Load coordinates for 4 atom2s (using direct pointer access, no bounds checking)
		__m256d x2 = _mm256_set_pd(atoms_ptr[idx2[3]].x, atoms_ptr[idx2[2]].x, atoms_ptr[idx2[1]].x, atoms_ptr[idx2[0]].x);
		__m256d y2 = _mm256_set_pd(atoms_ptr[idx2[3]].y, atoms_ptr[idx2[2]].y, atoms_ptr[idx2[1]].y, atoms_ptr[idx2[0]].y);
		__m256d z2 = _mm256_set_pd(atoms_ptr[idx2[3]].z, atoms_ptr[idx2[2]].z, atoms_ptr[idx2[1]].z, atoms_ptr[idx2[0]].z);
		
		// Calculate dx, dy, dz for 4 pairs
		__m256d dx = _mm256_sub_pd(x1, x2);
		__m256d dy = _mm256_sub_pd(y1, y2);
		__m256d dz = _mm256_sub_pd(z1, z2);
		
		// Calculate r2 = dx*dx + dy*dy + dz*dz for 4 pairs
		__m256d dx2 = _mm256_mul_pd(dx, dx);
		__m256d dy2 = _mm256_mul_pd(dy, dy);
		__m256d dz2 = _mm256_mul_pd(dz, dz);
		__m256d r2_vec = _mm256_add_pd(_mm256_add_pd(dx2, dy2), dz2);
		
		// Load pair parameters (esQ, vdwA, vdwB) for 4 pairs
		__m256d esQ_vec = _mm256_set_pd(
			pair_amber[i+3].esQ,
			pair_amber[i+2].esQ,
			pair_amber[i+1].esQ,
			pair_amber[i+0].esQ
		);
		__m256d vdwA_vec = _mm256_set_pd(
			pair_amber[i+3].vdwA,
			pair_amber[i+2].vdwA,
			pair_amber[i+1].vdwA,
			pair_amber[i+0].vdwA
		);
		__m256d vdwB_vec = _mm256_set_pd(
			pair_amber[i+3].vdwB,
			pair_amber[i+2].vdwB,
			pair_amber[i+1].vdwB,
			pair_amber[i+0].vdwB
		);
		
		// Calculate 1/r² and r for vectorized energy computation
		__m256d one = _mm256_set1_pd(1.0);
		__m256d inv_r2_vec = _mm256_div_pd(one, r2_vec);  // 1/r²
		__m256d r_vec = _mm256_sqrt_pd(r2_vec);  // r = sqrt(r²)
		__m256d inv_r_vec = _mm256_div_pd(one, r_vec);  // 1/r
		
		// Calculate electrostatics energy
		// es = esQ/r² (if distance_dependent_dielectric) or esQ/r (otherwise)
		__m256d es_vec;
		if (params.distance_dependent_dielectric) {
			es_vec = _mm256_mul_pd(esQ_vec, inv_r2_vec);
		} else {
			es_vec = _mm256_mul_pd(esQ_vec, inv_r_vec);
		}
		
		// Calculate van der Waals energy: vdw = vdwA/r¹² - vdwB/r⁶
		// r⁶ = r² * r² * r²
		__m256d r6_vec = _mm256_mul_pd(_mm256_mul_pd(r2_vec, r2_vec), r2_vec);
		// r¹² = r⁶ * r⁶
		__m256d r12_vec = _mm256_mul_pd(r6_vec, r6_vec);
		// 1/r⁶ and 1/r¹²
		__m256d inv_r6_vec = _mm256_div_pd(one, r6_vec);
		__m256d inv_r12_vec = _mm256_div_pd(one, r12_vec);
		// vdw = vdwA/r¹² - vdwB/r⁶ = vdwA * (1/r¹²) - vdwB * (1/r⁶)
		__m256d vdw_vec = _mm256_fmsub_pd(vdwA_vec, inv_r12_vec, _mm256_mul_pd(vdwB_vec, inv_r6_vec));
		
		// Total energy per pair: es + vdw
		__m256d energy_vec = _mm256_add_pd(es_vec, vdw_vec);
		
		// Horizontal sum: add all 4 energy values
		// Extract to array and sum (horizontal reduction)
		alignas(32) double energy_array[4];
		_mm256_store_pd(energy_array, energy_vec);
		energy += energy_array[0] + energy_array[1] + energy_array[2] + energy_array[3];
	}
	
	// Handle remaining pairs (scalar)
	for (; i < num_amber; i++) {
		Real3<double> atom1 = atoms_ptr[pair_amber[i].atomi1];
		Real3<double> atom2 = atoms_ptr[pair_amber[i].atomi2];
		double r2 = distance_sq(atom1, atom2);
		assert(!::std::isnan(r2));
		double r = ::std::sqrt(r2);
		energy += pair_amber[i].calc(r, r2, params.distance_dependent_dielectric);
	}

	// Process eef1 interactions with SIMD (same pattern)
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
		
		// Validate indices
		for (int j = 0; j < 4; j++) {
			assert(idx1[j] >= 0 && idx1[j] < atoms_size);
			assert(idx2[j] >= 0 && idx2[j] < atoms_size);
		}
		
		// Load coordinates for 4 atom1s
		__m256d x1 = _mm256_set_pd(atoms_ptr[idx1[3]].x, atoms_ptr[idx1[2]].x, atoms_ptr[idx1[1]].x, atoms_ptr[idx1[0]].x);
		__m256d y1 = _mm256_set_pd(atoms_ptr[idx1[3]].y, atoms_ptr[idx1[2]].y, atoms_ptr[idx1[1]].y, atoms_ptr[idx1[0]].y);
		__m256d z1 = _mm256_set_pd(atoms_ptr[idx1[3]].z, atoms_ptr[idx1[2]].z, atoms_ptr[idx1[1]].z, atoms_ptr[idx1[0]].z);
		
		// Load coordinates for 4 atom2s
		__m256d x2 = _mm256_set_pd(atoms_ptr[idx2[3]].x, atoms_ptr[idx2[2]].x, atoms_ptr[idx2[1]].x, atoms_ptr[idx2[0]].x);
		__m256d y2 = _mm256_set_pd(atoms_ptr[idx2[3]].y, atoms_ptr[idx2[2]].y, atoms_ptr[idx2[1]].y, atoms_ptr[idx2[0]].y);
		__m256d z2 = _mm256_set_pd(atoms_ptr[idx2[3]].z, atoms_ptr[idx2[2]].z, atoms_ptr[idx2[1]].z, atoms_ptr[idx2[0]].z);
		
		// Calculate dx, dy, dz for 4 pairs
		__m256d dx = _mm256_sub_pd(x1, x2);
		__m256d dy = _mm256_sub_pd(y1, y2);
		__m256d dz = _mm256_sub_pd(z1, z2);
		
		// Calculate r2 = dx*dx + dy*dy + dz*dz for 4 pairs
		__m256d dx2 = _mm256_mul_pd(dx, dx);
		__m256d dy2 = _mm256_mul_pd(dy, dy);
		__m256d dz2 = _mm256_mul_pd(dz, dz);
		__m256d r2_vec = _mm256_add_pd(_mm256_add_pd(dx2, dy2), dz2);
		
		// Calculate r = sqrt(r2) for vectorized computation
		__m256d r_vec = _mm256_sqrt_pd(r2_vec);
		__m256d one = _mm256_set1_pd(1.0);
		__m256d cutoff = _mm256_set1_pd(9.0);
		__m256d inv_r2_vec = _mm256_div_pd(one, r2_vec);
		
		// Load pair parameters for 4 pairs
		__m256d vdwRadius1_vec = _mm256_set_pd(
			pair_eef1[i+3].vdwRadius1,
			pair_eef1[i+2].vdwRadius1,
			pair_eef1[i+1].vdwRadius1,
			pair_eef1[i+0].vdwRadius1
		);
		__m256d lambda1_vec = _mm256_set_pd(
			pair_eef1[i+3].lambda1,
			pair_eef1[i+2].lambda1,
			pair_eef1[i+1].lambda1,
			pair_eef1[i+0].lambda1
		);
		__m256d alpha1_vec = _mm256_set_pd(
			pair_eef1[i+3].alpha1,
			pair_eef1[i+2].alpha1,
			pair_eef1[i+1].alpha1,
			pair_eef1[i+0].alpha1
		);
		__m256d vdwRadius2_vec = _mm256_set_pd(
			pair_eef1[i+3].vdwRadius2,
			pair_eef1[i+2].vdwRadius2,
			pair_eef1[i+1].vdwRadius2,
			pair_eef1[i+0].vdwRadius2
		);
		__m256d lambda2_vec = _mm256_set_pd(
			pair_eef1[i+3].lambda2,
			pair_eef1[i+2].lambda2,
			pair_eef1[i+1].lambda2,
			pair_eef1[i+0].lambda2
		);
		__m256d alpha2_vec = _mm256_set_pd(
			pair_eef1[i+3].alpha2,
			pair_eef1[i+2].alpha2,
			pair_eef1[i+1].alpha2,
			pair_eef1[i+0].alpha2
		);
		
		// Check cutoff: if r > 9.0, set mask to zero
		__m256d cutoff_mask = _mm256_cmp_pd(r_vec, cutoff, _CMP_LE_OQ);  // 1.0 if r <= 9.0, else 0.0
		
		// Calculate Xij = (r - vdwRadius1) / lambda1
		__m256d Xij_vec = _mm256_div_pd(_mm256_sub_pd(r_vec, vdwRadius1_vec), lambda1_vec);
		// Calculate Xji = (r - vdwRadius2) / lambda2
		__m256d Xji_vec = _mm256_div_pd(_mm256_sub_pd(r_vec, vdwRadius2_vec), lambda2_vec);
		
		// Calculate -Xij*Xij and -Xji*Xji
		__m256d neg_Xij2_vec = _mm256_mul_pd(_mm256_set1_pd(-1.0), _mm256_mul_pd(Xij_vec, Xij_vec));
		__m256d neg_Xji2_vec = _mm256_mul_pd(_mm256_set1_pd(-1.0), _mm256_mul_pd(Xji_vec, Xji_vec));
		
		// Use exact std::exp() to preserve force field accuracy
		// Extract to scalars, compute exact exp, reload to vectors
		alignas(32) double neg_Xij2_array[4], neg_Xji2_array[4];
		_mm256_store_pd(neg_Xij2_array, neg_Xij2_vec);
		_mm256_store_pd(neg_Xji2_array, neg_Xji2_vec);
		
		alignas(32) double exp_Xij2_array[4], exp_Xji2_array[4];
		for (int j = 0; j < 4; j++) {
			exp_Xij2_array[j] = ::std::exp(neg_Xij2_array[j]);
			exp_Xji2_array[j] = ::std::exp(neg_Xji2_array[j]);
		}
		__m256d exp_Xij2_vec = _mm256_load_pd(exp_Xij2_array);
		__m256d exp_Xji2_vec = _mm256_load_pd(exp_Xji2_array);
		
		// Calculate energy: -(alpha1*exp(-Xij²) + alpha2*exp(-Xji²)) / r²
		__m256d energy_vec = _mm256_mul_pd(
			_mm256_set1_pd(-1.0),
			_mm256_mul_pd(
				inv_r2_vec,
				_mm256_add_pd(
					_mm256_mul_pd(alpha1_vec, exp_Xij2_vec),
					_mm256_mul_pd(alpha2_vec, exp_Xji2_vec)
				)
			)
		);
		
		// Apply cutoff mask: zero out pairs where r > 9.0
		energy_vec = _mm256_and_pd(energy_vec, cutoff_mask);
		
		// Horizontal sum
		alignas(32) double energy_array[4];
		_mm256_store_pd(energy_array, energy_vec);
		energy += energy_array[0] + energy_array[1] + energy_array[2] + energy_array[3];
	}
	
	// Handle remaining pairs (scalar)
	for (; i < num_eef1; i++) {
		Real3<double> atom1 = atoms_ptr[pair_eef1[i].atomi1];
		Real3<double> atom2 = atoms_ptr[pair_eef1[i].atomi2];
		double r2 = distance_sq(atom1, atom2);
		assert(!::std::isnan(r2));
		double r = ::std::sqrt(r2);
		energy += pair_eef1[i].calc(r, r2);
	}

	return energy;
}

#ifdef USE_AVX512
// AVX-512 version: process 8 atom pairs simultaneously
// Use attribute target to allow multiple versions to coexist
inline __attribute__((target("avx512f,avx512dq,fma")))
double calc_avx512(const Array<Real3<double>> & atoms, const Params & params, const AtomPairs & pairs) {
	double energy = 0.0;

	// Get raw pointer to avoid bounds checking overhead in hot loop
	const Real3<double>* atoms_ptr = atoms.pointer();
	int64_t atoms_size = atoms.get_size();

	// Process amber interactions with SIMD
	auto pair_amber = reinterpret_cast<const AtomPairAmber<double> *>(&pairs + 1);
	int num_amber = pairs.num_amber;
	
	// SIMD loop: process 8 pairs at a time (AVX-512)
	int i = 0;
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
		__m512d x1 = _mm512_set_pd(atoms_ptr[idx1[7]].x, atoms_ptr[idx1[6]].x, atoms_ptr[idx1[5]].x, atoms_ptr[idx1[4]].x,
		                           atoms_ptr[idx1[3]].x, atoms_ptr[idx1[2]].x, atoms_ptr[idx1[1]].x, atoms_ptr[idx1[0]].x);
		__m512d y1 = _mm512_set_pd(atoms_ptr[idx1[7]].y, atoms_ptr[idx1[6]].y, atoms_ptr[idx1[5]].y, atoms_ptr[idx1[4]].y,
		                           atoms_ptr[idx1[3]].y, atoms_ptr[idx1[2]].y, atoms_ptr[idx1[1]].y, atoms_ptr[idx1[0]].y);
		__m512d z1 = _mm512_set_pd(atoms_ptr[idx1[7]].z, atoms_ptr[idx1[6]].z, atoms_ptr[idx1[5]].z, atoms_ptr[idx1[4]].z,
		                           atoms_ptr[idx1[3]].z, atoms_ptr[idx1[2]].z, atoms_ptr[idx1[1]].z, atoms_ptr[idx1[0]].z);
		
		// Load coordinates for 8 atom2s
		__m512d x2 = _mm512_set_pd(atoms_ptr[idx2[7]].x, atoms_ptr[idx2[6]].x, atoms_ptr[idx2[5]].x, atoms_ptr[idx2[4]].x,
		                           atoms_ptr[idx2[3]].x, atoms_ptr[idx2[2]].x, atoms_ptr[idx2[1]].x, atoms_ptr[idx2[0]].x);
		__m512d y2 = _mm512_set_pd(atoms_ptr[idx2[7]].y, atoms_ptr[idx2[6]].y, atoms_ptr[idx2[5]].y, atoms_ptr[idx2[4]].y,
		                           atoms_ptr[idx2[3]].y, atoms_ptr[idx2[2]].y, atoms_ptr[idx2[1]].y, atoms_ptr[idx2[0]].y);
		__m512d z2 = _mm512_set_pd(atoms_ptr[idx2[7]].z, atoms_ptr[idx2[6]].z, atoms_ptr[idx2[5]].z, atoms_ptr[idx2[4]].z,
		                           atoms_ptr[idx2[3]].z, atoms_ptr[idx2[2]].z, atoms_ptr[idx2[1]].z, atoms_ptr[idx2[0]].z);
		
		// Calculate dx, dy, dz for 8 pairs
		__m512d dx = _mm512_sub_pd(x1, x2);
		__m512d dy = _mm512_sub_pd(y1, y2);
		__m512d dz = _mm512_sub_pd(z1, z2);
		
		// Calculate r2 = dx*dx + dy*dy + dz*dz for 8 pairs
		__m512d dx2 = _mm512_mul_pd(dx, dx);
		__m512d dy2 = _mm512_mul_pd(dy, dy);
		__m512d dz2 = _mm512_mul_pd(dz, dz);
		__m512d r2_vec = _mm512_add_pd(_mm512_add_pd(dx2, dy2), dz2);
		
		// Load pair parameters (esQ, vdwA, vdwB) for 8 pairs
		__m512d esQ_vec = _mm512_set_pd(
			pair_amber[i+7].esQ, pair_amber[i+6].esQ, pair_amber[i+5].esQ, pair_amber[i+4].esQ,
			pair_amber[i+3].esQ, pair_amber[i+2].esQ, pair_amber[i+1].esQ, pair_amber[i+0].esQ
		);
		__m512d vdwA_vec = _mm512_set_pd(
			pair_amber[i+7].vdwA, pair_amber[i+6].vdwA, pair_amber[i+5].vdwA, pair_amber[i+4].vdwA,
			pair_amber[i+3].vdwA, pair_amber[i+2].vdwA, pair_amber[i+1].vdwA, pair_amber[i+0].vdwA
		);
		__m512d vdwB_vec = _mm512_set_pd(
			pair_amber[i+7].vdwB, pair_amber[i+6].vdwB, pair_amber[i+5].vdwB, pair_amber[i+4].vdwB,
			pair_amber[i+3].vdwB, pair_amber[i+2].vdwB, pair_amber[i+1].vdwB, pair_amber[i+0].vdwB
		);
		
		// Calculate 1/r² and r for vectorized energy computation
		__m512d one = _mm512_set1_pd(1.0);
		__m512d inv_r2_vec = _mm512_div_pd(one, r2_vec);  // 1/r²
		__m512d r_vec = _mm512_sqrt_pd(r2_vec);  // r = sqrt(r²)
		__m512d inv_r_vec = _mm512_div_pd(one, r_vec);  // 1/r
		
		// Calculate electrostatics energy
		// es = esQ/r² (if distance_dependent_dielectric) or esQ/r (otherwise)
		__m512d es_vec;
		if (params.distance_dependent_dielectric) {
			es_vec = _mm512_mul_pd(esQ_vec, inv_r2_vec);
		} else {
			es_vec = _mm512_mul_pd(esQ_vec, inv_r_vec);
		}
		
		// Calculate van der Waals energy: vdw = vdwA/r¹² - vdwB/r⁶
		// r⁶ = r² * r² * r²
		__m512d r6_vec = _mm512_mul_pd(_mm512_mul_pd(r2_vec, r2_vec), r2_vec);
		// r¹² = r⁶ * r⁶
		__m512d r12_vec = _mm512_mul_pd(r6_vec, r6_vec);
		// 1/r⁶ and 1/r¹²
		__m512d inv_r6_vec = _mm512_div_pd(one, r6_vec);
		__m512d inv_r12_vec = _mm512_div_pd(one, r12_vec);
		// vdw = vdwA/r¹² - vdwB/r⁶ = vdwA * (1/r¹²) - vdwB * (1/r⁶)
		__m512d vdw_vec = _mm512_fmsub_pd(vdwA_vec, inv_r12_vec, _mm512_mul_pd(vdwB_vec, inv_r6_vec));
		
		// Total energy per pair: es + vdw
		__m512d energy_vec = _mm512_add_pd(es_vec, vdw_vec);
		
		// Horizontal sum: add all 8 energy values
		// Extract to array and sum (manual horizontal reduction)
		alignas(64) double energy_array[8];
		_mm512_store_pd(energy_array, energy_vec);
		energy += energy_array[0] + energy_array[1] + energy_array[2] + energy_array[3] +
		          energy_array[4] + energy_array[5] + energy_array[6] + energy_array[7];
	}
	
	// Handle remaining pairs (scalar)
	for (; i < num_amber; i++) {
		Real3<double> atom1 = atoms_ptr[pair_amber[i].atomi1];
		Real3<double> atom2 = atoms_ptr[pair_amber[i].atomi2];
		double r2 = distance_sq(atom1, atom2);
		assert(!::std::isnan(r2));
		double r = ::std::sqrt(r2);
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
		__m512d x1 = _mm512_set_pd(atoms_ptr[idx1[7]].x, atoms_ptr[idx1[6]].x, atoms_ptr[idx1[5]].x, atoms_ptr[idx1[4]].x,
		                           atoms_ptr[idx1[3]].x, atoms_ptr[idx1[2]].x, atoms_ptr[idx1[1]].x, atoms_ptr[idx1[0]].x);
		__m512d y1 = _mm512_set_pd(atoms_ptr[idx1[7]].y, atoms_ptr[idx1[6]].y, atoms_ptr[idx1[5]].y, atoms_ptr[idx1[4]].y,
		                           atoms_ptr[idx1[3]].y, atoms_ptr[idx1[2]].y, atoms_ptr[idx1[1]].y, atoms_ptr[idx1[0]].y);
		__m512d z1 = _mm512_set_pd(atoms_ptr[idx1[7]].z, atoms_ptr[idx1[6]].z, atoms_ptr[idx1[5]].z, atoms_ptr[idx1[4]].z,
		                           atoms_ptr[idx1[3]].z, atoms_ptr[idx1[2]].z, atoms_ptr[idx1[1]].z, atoms_ptr[idx1[0]].z);
		
		// Load coordinates for 8 atom2s
		__m512d x2 = _mm512_set_pd(atoms_ptr[idx2[7]].x, atoms_ptr[idx2[6]].x, atoms_ptr[idx2[5]].x, atoms_ptr[idx2[4]].x,
		                           atoms_ptr[idx2[3]].x, atoms_ptr[idx2[2]].x, atoms_ptr[idx2[1]].x, atoms_ptr[idx2[0]].x);
		__m512d y2 = _mm512_set_pd(atoms_ptr[idx2[7]].y, atoms_ptr[idx2[6]].y, atoms_ptr[idx2[5]].y, atoms_ptr[idx2[4]].y,
		                           atoms_ptr[idx2[3]].y, atoms_ptr[idx2[2]].y, atoms_ptr[idx2[1]].y, atoms_ptr[idx2[0]].y);
		__m512d z2 = _mm512_set_pd(atoms_ptr[idx2[7]].z, atoms_ptr[idx2[6]].z, atoms_ptr[idx2[5]].z, atoms_ptr[idx2[4]].z,
		                           atoms_ptr[idx2[3]].z, atoms_ptr[idx2[2]].z, atoms_ptr[idx2[1]].z, atoms_ptr[idx2[0]].z);
		
		// Calculate dx, dy, dz for 8 pairs
		__m512d dx = _mm512_sub_pd(x1, x2);
		__m512d dy = _mm512_sub_pd(y1, y2);
		__m512d dz = _mm512_sub_pd(z1, z2);
		
		// Calculate r2 = dx*dx + dy*dy + dz*dz for 8 pairs
		__m512d dx2 = _mm512_mul_pd(dx, dx);
		__m512d dy2 = _mm512_mul_pd(dy, dy);
		__m512d dz2 = _mm512_mul_pd(dz, dz);
		__m512d r2_vec = _mm512_add_pd(_mm512_add_pd(dx2, dy2), dz2);
		
		// Calculate r = sqrt(r2) for vectorized computation
		__m512d r_vec = _mm512_sqrt_pd(r2_vec);
		__m512d one = _mm512_set1_pd(1.0);
		__m512d cutoff = _mm512_set1_pd(9.0);
		__m512d inv_r2_vec = _mm512_div_pd(one, r2_vec);
		
		// Load pair parameters for 8 pairs
		__m512d vdwRadius1_vec = _mm512_set_pd(
			pair_eef1[i+7].vdwRadius1, pair_eef1[i+6].vdwRadius1, pair_eef1[i+5].vdwRadius1, pair_eef1[i+4].vdwRadius1,
			pair_eef1[i+3].vdwRadius1, pair_eef1[i+2].vdwRadius1, pair_eef1[i+1].vdwRadius1, pair_eef1[i+0].vdwRadius1
		);
		__m512d lambda1_vec = _mm512_set_pd(
			pair_eef1[i+7].lambda1, pair_eef1[i+6].lambda1, pair_eef1[i+5].lambda1, pair_eef1[i+4].lambda1,
			pair_eef1[i+3].lambda1, pair_eef1[i+2].lambda1, pair_eef1[i+1].lambda1, pair_eef1[i+0].lambda1
		);
		__m512d alpha1_vec = _mm512_set_pd(
			pair_eef1[i+7].alpha1, pair_eef1[i+6].alpha1, pair_eef1[i+5].alpha1, pair_eef1[i+4].alpha1,
			pair_eef1[i+3].alpha1, pair_eef1[i+2].alpha1, pair_eef1[i+1].alpha1, pair_eef1[i+0].alpha1
		);
		__m512d vdwRadius2_vec = _mm512_set_pd(
			pair_eef1[i+7].vdwRadius2, pair_eef1[i+6].vdwRadius2, pair_eef1[i+5].vdwRadius2, pair_eef1[i+4].vdwRadius2,
			pair_eef1[i+3].vdwRadius2, pair_eef1[i+2].vdwRadius2, pair_eef1[i+1].vdwRadius2, pair_eef1[i+0].vdwRadius2
		);
		__m512d lambda2_vec = _mm512_set_pd(
			pair_eef1[i+7].lambda2, pair_eef1[i+6].lambda2, pair_eef1[i+5].lambda2, pair_eef1[i+4].lambda2,
			pair_eef1[i+3].lambda2, pair_eef1[i+2].lambda2, pair_eef1[i+1].lambda2, pair_eef1[i+0].lambda2
		);
		__m512d alpha2_vec = _mm512_set_pd(
			pair_eef1[i+7].alpha2, pair_eef1[i+6].alpha2, pair_eef1[i+5].alpha2, pair_eef1[i+4].alpha2,
			pair_eef1[i+3].alpha2, pair_eef1[i+2].alpha2, pair_eef1[i+1].alpha2, pair_eef1[i+0].alpha2
		);
		
		// Check cutoff: if r > 9.0, set mask to zero
		__mmask8 cutoff_mask = _mm512_cmp_pd_mask(r_vec, cutoff, _CMP_LE_OQ);
		
		// Calculate Xij = (r - vdwRadius1) / lambda1
		__m512d Xij_vec = _mm512_div_pd(_mm512_sub_pd(r_vec, vdwRadius1_vec), lambda1_vec);
		// Calculate Xji = (r - vdwRadius2) / lambda2
		__m512d Xji_vec = _mm512_div_pd(_mm512_sub_pd(r_vec, vdwRadius2_vec), lambda2_vec);
		
		// Calculate -Xij*Xij and -Xji*Xji
		__m512d neg_Xij2_vec = _mm512_mul_pd(_mm512_set1_pd(-1.0), _mm512_mul_pd(Xij_vec, Xij_vec));
		__m512d neg_Xji2_vec = _mm512_mul_pd(_mm512_set1_pd(-1.0), _mm512_mul_pd(Xji_vec, Xji_vec));
		
		// Use exact std::exp() to preserve force field accuracy
		// Extract to scalars, compute exact exp, reload to vectors
		alignas(64) double neg_Xij2_array[8], neg_Xji2_array[8];
		_mm512_store_pd(neg_Xij2_array, neg_Xij2_vec);
		_mm512_store_pd(neg_Xji2_array, neg_Xji2_vec);
		
		alignas(64) double exp_Xij2_array[8], exp_Xji2_array[8];
		for (int j = 0; j < 8; j++) {
			exp_Xij2_array[j] = ::std::exp(neg_Xij2_array[j]);
			exp_Xji2_array[j] = ::std::exp(neg_Xji2_array[j]);
		}
		__m512d exp_Xij2_vec = _mm512_load_pd(exp_Xij2_array);
		__m512d exp_Xji2_vec = _mm512_load_pd(exp_Xji2_array);
		
		// Calculate energy: -(alpha1*exp(-Xij²) + alpha2*exp(-Xji²)) / r²
		__m512d energy_vec = _mm512_mul_pd(
			_mm512_set1_pd(-1.0),
			_mm512_mul_pd(
				inv_r2_vec,
				_mm512_add_pd(
					_mm512_mul_pd(alpha1_vec, exp_Xij2_vec),
					_mm512_mul_pd(alpha2_vec, exp_Xji2_vec)
				)
			)
		);
		
		// Apply cutoff mask: zero out pairs where r > 9.0
		energy_vec = _mm512_maskz_mul_pd(cutoff_mask, energy_vec, _mm512_set1_pd(1.0));
		
		// Horizontal sum: add all 8 energy values
		alignas(64) double energy_array[8];
		_mm512_store_pd(energy_array, energy_vec);
		energy += energy_array[0] + energy_array[1] + energy_array[2] + energy_array[3] +
		          energy_array[4] + energy_array[5] + energy_array[6] + energy_array[7];
	}
	
	// Handle remaining pairs (scalar)
	for (; i < num_eef1; i++) {
		Real3<double> atom1 = atoms_ptr[pair_eef1[i].atomi1];
		Real3<double> atom2 = atoms_ptr[pair_eef1[i].atomi2];
		double r2 = distance_sq(atom1, atom2);
		assert(!::std::isnan(r2));
		double r = ::std::sqrt(r2);
		energy += pair_eef1[i].calc(r, r2);
	}

	return energy;
}
#endif // USE_AVX512

// AVX2 version with fast exp approximation (for testing/performance comparison)
__attribute__((target("avx2,fma")))
inline double calc_avx2_fast_exp(const Array<Real3<double>> & atoms, const Params & params, const AtomPairs & pairs) {
	double energy = 0.0;

	// Get raw pointer to avoid bounds checking overhead in hot loop
	const Real3<double>* atoms_ptr = atoms.pointer();
	int64_t atoms_size = atoms.get_size();

	// Process amber interactions with SIMD (same as calc_avx2)
	auto pair_amber = reinterpret_cast<const AtomPairAmber<double> *>(&pairs + 1);
	int num_amber = pairs.num_amber;
	
	int i = 0;
	for (; i + 3 < num_amber; i += 4) {
		int idx1[4] = {pair_amber[i+0].atomi1, pair_amber[i+1].atomi1, pair_amber[i+2].atomi1, pair_amber[i+3].atomi1};
		int idx2[4] = {pair_amber[i+0].atomi2, pair_amber[i+1].atomi2, pair_amber[i+2].atomi2, pair_amber[i+3].atomi2};
		
		for (int j = 0; j < 4; j++) {
			assert(idx1[j] >= 0 && idx1[j] < atoms_size);
			assert(idx2[j] >= 0 && idx2[j] < atoms_size);
		}
		
		__m256d x1 = _mm256_set_pd(atoms_ptr[idx1[3]].x, atoms_ptr[idx1[2]].x, atoms_ptr[idx1[1]].x, atoms_ptr[idx1[0]].x);
		__m256d y1 = _mm256_set_pd(atoms_ptr[idx1[3]].y, atoms_ptr[idx1[2]].y, atoms_ptr[idx1[1]].y, atoms_ptr[idx1[0]].y);
		__m256d z1 = _mm256_set_pd(atoms_ptr[idx1[3]].z, atoms_ptr[idx1[2]].z, atoms_ptr[idx1[1]].z, atoms_ptr[idx1[0]].z);
		__m256d x2 = _mm256_set_pd(atoms_ptr[idx2[3]].x, atoms_ptr[idx2[2]].x, atoms_ptr[idx2[1]].x, atoms_ptr[idx2[0]].x);
		__m256d y2 = _mm256_set_pd(atoms_ptr[idx2[3]].y, atoms_ptr[idx2[2]].y, atoms_ptr[idx2[1]].y, atoms_ptr[idx2[0]].y);
		__m256d z2 = _mm256_set_pd(atoms_ptr[idx2[3]].z, atoms_ptr[idx2[2]].z, atoms_ptr[idx2[1]].z, atoms_ptr[idx2[0]].z);
		
		__m256d dx = _mm256_sub_pd(x1, x2);
		__m256d dy = _mm256_sub_pd(y1, y2);
		__m256d dz = _mm256_sub_pd(z1, z2);
		__m256d dx2 = _mm256_mul_pd(dx, dx);
		__m256d dy2 = _mm256_mul_pd(dy, dy);
		__m256d dz2 = _mm256_mul_pd(dz, dz);
		__m256d r2_vec = _mm256_add_pd(_mm256_add_pd(dx2, dy2), dz2);
		
		__m256d esQ_vec = _mm256_set_pd(pair_amber[i+3].esQ, pair_amber[i+2].esQ, pair_amber[i+1].esQ, pair_amber[i+0].esQ);
		__m256d vdwA_vec = _mm256_set_pd(pair_amber[i+3].vdwA, pair_amber[i+2].vdwA, pair_amber[i+1].vdwA, pair_amber[i+0].vdwA);
		__m256d vdwB_vec = _mm256_set_pd(pair_amber[i+3].vdwB, pair_amber[i+2].vdwB, pair_amber[i+1].vdwB, pair_amber[i+0].vdwB);
		
		__m256d one = _mm256_set1_pd(1.0);
		__m256d inv_r2_vec = _mm256_div_pd(one, r2_vec);
		__m256d r_vec = _mm256_sqrt_pd(r2_vec);
		__m256d inv_r_vec = _mm256_div_pd(one, r_vec);
		
		__m256d es_vec;
		if (params.distance_dependent_dielectric) {
			es_vec = _mm256_mul_pd(esQ_vec, inv_r2_vec);
		} else {
			es_vec = _mm256_mul_pd(esQ_vec, inv_r_vec);
		}
		
		__m256d r6_vec = _mm256_mul_pd(_mm256_mul_pd(r2_vec, r2_vec), r2_vec);
		__m256d r12_vec = _mm256_mul_pd(r6_vec, r6_vec);
		__m256d inv_r6_vec = _mm256_div_pd(one, r6_vec);
		__m256d inv_r12_vec = _mm256_div_pd(one, r12_vec);
		__m256d vdw_vec = _mm256_fmsub_pd(vdwA_vec, inv_r12_vec, _mm256_mul_pd(vdwB_vec, inv_r6_vec));
		__m256d energy_vec = _mm256_add_pd(es_vec, vdw_vec);
		
		alignas(32) double energy_array[4];
		_mm256_store_pd(energy_array, energy_vec);
		energy += energy_array[0] + energy_array[1] + energy_array[2] + energy_array[3];
	}
	
	for (; i < num_amber; i++) {
		Real3<double> atom1 = atoms[pair_amber[i].atomi1];
		Real3<double> atom2 = atoms[pair_amber[i].atomi2];
		double r2 = distance_sq(atom1, atom2);
		assert(!::std::isnan(r2));
		double r = ::std::sqrt(r2);
		energy += pair_amber[i].calc(r, r2, params.distance_dependent_dielectric);
	}

	// Process eef1 interactions with fast exp approximation
	auto pair_eef1 = reinterpret_cast<const AtomPairEef1<double> *>(pair_amber + num_amber);
	int num_eef1 = pairs.num_eef1;
	
	i = 0;
	for (; i + 3 < num_eef1; i += 4) {
		int idx1[4] = {pair_eef1[i+0].atomi1, pair_eef1[i+1].atomi1, pair_eef1[i+2].atomi1, pair_eef1[i+3].atomi1};
		int idx2[4] = {pair_eef1[i+0].atomi2, pair_eef1[i+1].atomi2, pair_eef1[i+2].atomi2, pair_eef1[i+3].atomi2};
		
		for (int j = 0; j < 4; j++) {
			assert(idx1[j] >= 0 && idx1[j] < atoms_size);
			assert(idx2[j] >= 0 && idx2[j] < atoms_size);
		}
		
		__m256d x1 = _mm256_set_pd(atoms_ptr[idx1[3]].x, atoms_ptr[idx1[2]].x, atoms_ptr[idx1[1]].x, atoms_ptr[idx1[0]].x);
		__m256d y1 = _mm256_set_pd(atoms_ptr[idx1[3]].y, atoms_ptr[idx1[2]].y, atoms_ptr[idx1[1]].y, atoms_ptr[idx1[0]].y);
		__m256d z1 = _mm256_set_pd(atoms_ptr[idx1[3]].z, atoms_ptr[idx1[2]].z, atoms_ptr[idx1[1]].z, atoms_ptr[idx1[0]].z);
		__m256d x2 = _mm256_set_pd(atoms_ptr[idx2[3]].x, atoms_ptr[idx2[2]].x, atoms_ptr[idx2[1]].x, atoms_ptr[idx2[0]].x);
		__m256d y2 = _mm256_set_pd(atoms_ptr[idx2[3]].y, atoms_ptr[idx2[2]].y, atoms_ptr[idx2[1]].y, atoms_ptr[idx2[0]].y);
		__m256d z2 = _mm256_set_pd(atoms_ptr[idx2[3]].z, atoms_ptr[idx2[2]].z, atoms_ptr[idx2[1]].z, atoms_ptr[idx2[0]].z);
		
		__m256d dx = _mm256_sub_pd(x1, x2);
		__m256d dy = _mm256_sub_pd(y1, y2);
		__m256d dz = _mm256_sub_pd(z1, z2);
		__m256d dx2 = _mm256_mul_pd(dx, dx);
		__m256d dy2 = _mm256_mul_pd(dy, dy);
		__m256d dz2 = _mm256_mul_pd(dz, dz);
		__m256d r2_vec = _mm256_add_pd(_mm256_add_pd(dx2, dy2), dz2);
		
		__m256d r_vec = _mm256_sqrt_pd(r2_vec);
		__m256d one = _mm256_set1_pd(1.0);
		__m256d cutoff = _mm256_set1_pd(9.0);
		__m256d inv_r2_vec = _mm256_div_pd(one, r2_vec);
		
		__m256d vdwRadius1_vec = _mm256_set_pd(pair_eef1[i+3].vdwRadius1, pair_eef1[i+2].vdwRadius1, pair_eef1[i+1].vdwRadius1, pair_eef1[i+0].vdwRadius1);
		__m256d lambda1_vec = _mm256_set_pd(pair_eef1[i+3].lambda1, pair_eef1[i+2].lambda1, pair_eef1[i+1].lambda1, pair_eef1[i+0].lambda1);
		__m256d alpha1_vec = _mm256_set_pd(pair_eef1[i+3].alpha1, pair_eef1[i+2].alpha1, pair_eef1[i+1].alpha1, pair_eef1[i+0].alpha1);
		__m256d vdwRadius2_vec = _mm256_set_pd(pair_eef1[i+3].vdwRadius2, pair_eef1[i+2].vdwRadius2, pair_eef1[i+1].vdwRadius2, pair_eef1[i+0].vdwRadius2);
		__m256d lambda2_vec = _mm256_set_pd(pair_eef1[i+3].lambda2, pair_eef1[i+2].lambda2, pair_eef1[i+1].lambda2, pair_eef1[i+0].lambda2);
		__m256d alpha2_vec = _mm256_set_pd(pair_eef1[i+3].alpha2, pair_eef1[i+2].alpha2, pair_eef1[i+1].alpha2, pair_eef1[i+0].alpha2);
		
		__m256d cutoff_mask = _mm256_cmp_pd(r_vec, cutoff, _CMP_LE_OQ);
		__m256d Xij_vec = _mm256_div_pd(_mm256_sub_pd(r_vec, vdwRadius1_vec), lambda1_vec);
		__m256d Xji_vec = _mm256_div_pd(_mm256_sub_pd(r_vec, vdwRadius2_vec), lambda2_vec);
		
		__m256d neg_Xij2_vec = _mm256_mul_pd(_mm256_set1_pd(-1.0), _mm256_mul_pd(Xij_vec, Xij_vec));
		__m256d neg_Xji2_vec = _mm256_mul_pd(_mm256_set1_pd(-1.0), _mm256_mul_pd(Xji_vec, Xji_vec));
		
		// Fast exp approximation: Taylor series exp(x) ≈ 1 + x + x²/2 + x³/6 + x⁴/24 + x⁵/120
		double exp_limit_val = 20.0;
		__m256d exp_limit = _mm256_set1_pd(exp_limit_val);
		__m256d x_ij = _mm256_max_pd(neg_Xij2_vec, _mm256_set1_pd(-exp_limit_val));
		__m256d x_ji = _mm256_max_pd(neg_Xji2_vec, _mm256_set1_pd(-exp_limit_val));
		
		__m256d x2_ij = _mm256_mul_pd(x_ij, x_ij);
		__m256d x3_ij = _mm256_mul_pd(x2_ij, x_ij);
		__m256d x4_ij = _mm256_mul_pd(x3_ij, x_ij);
		__m256d x5_ij = _mm256_mul_pd(x4_ij, x_ij);
		__m256d exp_Xij2_vec = _mm256_fmadd_pd(x5_ij, _mm256_set1_pd(1.0/120.0),
			_mm256_fmadd_pd(x4_ij, _mm256_set1_pd(1.0/24.0),
				_mm256_fmadd_pd(x3_ij, _mm256_set1_pd(1.0/6.0),
					_mm256_fmadd_pd(x2_ij, _mm256_set1_pd(0.5),
						_mm256_add_pd(x_ij, _mm256_set1_pd(1.0))))));
		
		__m256d x2_ji = _mm256_mul_pd(x_ji, x_ji);
		__m256d x3_ji = _mm256_mul_pd(x2_ji, x_ji);
		__m256d x4_ji = _mm256_mul_pd(x3_ji, x_ji);
		__m256d x5_ji = _mm256_mul_pd(x4_ji, x_ji);
		__m256d exp_Xji2_vec = _mm256_fmadd_pd(x5_ji, _mm256_set1_pd(1.0/120.0),
			_mm256_fmadd_pd(x4_ji, _mm256_set1_pd(1.0/24.0),
				_mm256_fmadd_pd(x3_ji, _mm256_set1_pd(1.0/6.0),
					_mm256_fmadd_pd(x2_ji, _mm256_set1_pd(0.5),
						_mm256_add_pd(x_ji, _mm256_set1_pd(1.0))))));
		
		__m256d energy_vec = _mm256_mul_pd(_mm256_set1_pd(-1.0),
			_mm256_mul_pd(inv_r2_vec,
				_mm256_add_pd(_mm256_mul_pd(alpha1_vec, exp_Xij2_vec),
					_mm256_mul_pd(alpha2_vec, exp_Xji2_vec))));
		energy_vec = _mm256_and_pd(energy_vec, cutoff_mask);
		
		alignas(32) double energy_array[4];
		_mm256_store_pd(energy_array, energy_vec);
		energy += energy_array[0] + energy_array[1] + energy_array[2] + energy_array[3];
	}
	
	for (; i < num_eef1; i++) {
		Real3<double> atom1 = atoms[pair_eef1[i].atomi1];
		Real3<double> atom2 = atoms[pair_eef1[i].atomi2];
		double r2 = distance_sq(atom1, atom2);
		assert(!::std::isnan(r2));
		double r = ::std::sqrt(r2);
		energy += pair_eef1[i].calc(r, r2);
	}

	return energy;
}

#ifdef USE_AVX512
// AVX-512 version with fast exp approximation (for testing/performance comparison)
__attribute__((target("avx512f,avx512dq,fma")))
inline double calc_avx512_fast_exp(const Array<Real3<double>> & atoms, const Params & params, const AtomPairs & pairs) {
	double energy = 0.0;

	// Get raw pointer to avoid bounds checking overhead in hot loop
	const Real3<double>* atoms_ptr = atoms.pointer();
	int64_t atoms_size = atoms.get_size();

	auto pair_amber = reinterpret_cast<const AtomPairAmber<double> *>(&pairs + 1);
	int num_amber = pairs.num_amber;
	
	int i = 0;
	for (; i + 7 < num_amber; i += 8) {
		int idx1[8] = {pair_amber[i+0].atomi1, pair_amber[i+1].atomi1, pair_amber[i+2].atomi1, pair_amber[i+3].atomi1,
		               pair_amber[i+4].atomi1, pair_amber[i+5].atomi1, pair_amber[i+6].atomi1, pair_amber[i+7].atomi1};
		int idx2[8] = {pair_amber[i+0].atomi2, pair_amber[i+1].atomi2, pair_amber[i+2].atomi2, pair_amber[i+3].atomi2,
		               pair_amber[i+4].atomi2, pair_amber[i+5].atomi2, pair_amber[i+6].atomi2, pair_amber[i+7].atomi2};
		
		for (int j = 0; j < 8; j++) {
			assert(idx1[j] >= 0 && idx1[j] < atoms_size);
			assert(idx2[j] >= 0 && idx2[j] < atoms_size);
		}
		
		__m512d x1 = _mm512_set_pd(atoms_ptr[idx1[7]].x, atoms_ptr[idx1[6]].x, atoms_ptr[idx1[5]].x, atoms_ptr[idx1[4]].x,
		                           atoms_ptr[idx1[3]].x, atoms_ptr[idx1[2]].x, atoms_ptr[idx1[1]].x, atoms_ptr[idx1[0]].x);
		__m512d y1 = _mm512_set_pd(atoms_ptr[idx1[7]].y, atoms_ptr[idx1[6]].y, atoms_ptr[idx1[5]].y, atoms_ptr[idx1[4]].y,
		                           atoms_ptr[idx1[3]].y, atoms_ptr[idx1[2]].y, atoms_ptr[idx1[1]].y, atoms_ptr[idx1[0]].y);
		__m512d z1 = _mm512_set_pd(atoms_ptr[idx1[7]].z, atoms_ptr[idx1[6]].z, atoms_ptr[idx1[5]].z, atoms_ptr[idx1[4]].z,
		                           atoms_ptr[idx1[3]].z, atoms_ptr[idx1[2]].z, atoms_ptr[idx1[1]].z, atoms_ptr[idx1[0]].z);
		__m512d x2 = _mm512_set_pd(atoms_ptr[idx2[7]].x, atoms_ptr[idx2[6]].x, atoms_ptr[idx2[5]].x, atoms_ptr[idx2[4]].x,
		                           atoms_ptr[idx2[3]].x, atoms_ptr[idx2[2]].x, atoms_ptr[idx2[1]].x, atoms_ptr[idx2[0]].x);
		__m512d y2 = _mm512_set_pd(atoms_ptr[idx2[7]].y, atoms_ptr[idx2[6]].y, atoms_ptr[idx2[5]].y, atoms_ptr[idx2[4]].y,
		                           atoms_ptr[idx2[3]].y, atoms_ptr[idx2[2]].y, atoms_ptr[idx2[1]].y, atoms_ptr[idx2[0]].y);
		__m512d z2 = _mm512_set_pd(atoms_ptr[idx2[7]].z, atoms_ptr[idx2[6]].z, atoms_ptr[idx2[5]].z, atoms_ptr[idx2[4]].z,
		                           atoms_ptr[idx2[3]].z, atoms_ptr[idx2[2]].z, atoms_ptr[idx2[1]].z, atoms_ptr[idx2[0]].z);
		
		__m512d dx = _mm512_sub_pd(x1, x2);
		__m512d dy = _mm512_sub_pd(y1, y2);
		__m512d dz = _mm512_sub_pd(z1, z2);
		__m512d dx2 = _mm512_mul_pd(dx, dx);
		__m512d dy2 = _mm512_mul_pd(dy, dy);
		__m512d dz2 = _mm512_mul_pd(dz, dz);
		__m512d r2_vec = _mm512_add_pd(_mm512_add_pd(dx2, dy2), dz2);
		
		__m512d esQ_vec = _mm512_set_pd(pair_amber[i+7].esQ, pair_amber[i+6].esQ, pair_amber[i+5].esQ, pair_amber[i+4].esQ,
		                                pair_amber[i+3].esQ, pair_amber[i+2].esQ, pair_amber[i+1].esQ, pair_amber[i+0].esQ);
		__m512d vdwA_vec = _mm512_set_pd(pair_amber[i+7].vdwA, pair_amber[i+6].vdwA, pair_amber[i+5].vdwA, pair_amber[i+4].vdwA,
		                                 pair_amber[i+3].vdwA, pair_amber[i+2].vdwA, pair_amber[i+1].vdwA, pair_amber[i+0].vdwA);
		__m512d vdwB_vec = _mm512_set_pd(pair_amber[i+7].vdwB, pair_amber[i+6].vdwB, pair_amber[i+5].vdwB, pair_amber[i+4].vdwB,
		                                 pair_amber[i+3].vdwB, pair_amber[i+2].vdwB, pair_amber[i+1].vdwB, pair_amber[i+0].vdwB);
		
		__m512d one = _mm512_set1_pd(1.0);
		__m512d inv_r2_vec = _mm512_div_pd(one, r2_vec);
		__m512d r_vec = _mm512_sqrt_pd(r2_vec);
		__m512d inv_r_vec = _mm512_div_pd(one, r_vec);
		
		__m512d es_vec;
		if (params.distance_dependent_dielectric) {
			es_vec = _mm512_mul_pd(esQ_vec, inv_r2_vec);
		} else {
			es_vec = _mm512_mul_pd(esQ_vec, inv_r_vec);
		}
		
		__m512d r6_vec = _mm512_mul_pd(_mm512_mul_pd(r2_vec, r2_vec), r2_vec);
		__m512d r12_vec = _mm512_mul_pd(r6_vec, r6_vec);
		__m512d inv_r6_vec = _mm512_div_pd(one, r6_vec);
		__m512d inv_r12_vec = _mm512_div_pd(one, r12_vec);
		__m512d vdw_vec = _mm512_fmsub_pd(vdwA_vec, inv_r12_vec, _mm512_mul_pd(vdwB_vec, inv_r6_vec));
		__m512d energy_vec = _mm512_add_pd(es_vec, vdw_vec);
		
		alignas(64) double energy_array[8];
		_mm512_store_pd(energy_array, energy_vec);
		energy += energy_array[0] + energy_array[1] + energy_array[2] + energy_array[3] +
		          energy_array[4] + energy_array[5] + energy_array[6] + energy_array[7];
	}
	
	for (; i < num_amber; i++) {
		Real3<double> atom1 = atoms[pair_amber[i].atomi1];
		Real3<double> atom2 = atoms[pair_amber[i].atomi2];
		double r2 = distance_sq(atom1, atom2);
		assert(!::std::isnan(r2));
		double r = ::std::sqrt(r2);
		energy += pair_amber[i].calc(r, r2, params.distance_dependent_dielectric);
	}

	auto pair_eef1 = reinterpret_cast<const AtomPairEef1<double> *>(pair_amber + num_amber);
	int num_eef1 = pairs.num_eef1;
	
	i = 0;
	for (; i + 7 < num_eef1; i += 8) {
		int idx1[8] = {pair_eef1[i+0].atomi1, pair_eef1[i+1].atomi1, pair_eef1[i+2].atomi1, pair_eef1[i+3].atomi1,
		               pair_eef1[i+4].atomi1, pair_eef1[i+5].atomi1, pair_eef1[i+6].atomi1, pair_eef1[i+7].atomi1};
		int idx2[8] = {pair_eef1[i+0].atomi2, pair_eef1[i+1].atomi2, pair_eef1[i+2].atomi2, pair_eef1[i+3].atomi2,
		               pair_eef1[i+4].atomi2, pair_eef1[i+5].atomi2, pair_eef1[i+6].atomi2, pair_eef1[i+7].atomi2};
		
		for (int j = 0; j < 8; j++) {
			assert(idx1[j] >= 0 && idx1[j] < atoms_size);
			assert(idx2[j] >= 0 && idx2[j] < atoms_size);
		}
		
		__m512d x1 = _mm512_set_pd(atoms_ptr[idx1[7]].x, atoms_ptr[idx1[6]].x, atoms_ptr[idx1[5]].x, atoms_ptr[idx1[4]].x,
		                           atoms_ptr[idx1[3]].x, atoms_ptr[idx1[2]].x, atoms_ptr[idx1[1]].x, atoms_ptr[idx1[0]].x);
		__m512d y1 = _mm512_set_pd(atoms_ptr[idx1[7]].y, atoms_ptr[idx1[6]].y, atoms_ptr[idx1[5]].y, atoms_ptr[idx1[4]].y,
		                           atoms_ptr[idx1[3]].y, atoms_ptr[idx1[2]].y, atoms_ptr[idx1[1]].y, atoms_ptr[idx1[0]].y);
		__m512d z1 = _mm512_set_pd(atoms_ptr[idx1[7]].z, atoms_ptr[idx1[6]].z, atoms_ptr[idx1[5]].z, atoms_ptr[idx1[4]].z,
		                           atoms_ptr[idx1[3]].z, atoms_ptr[idx1[2]].z, atoms_ptr[idx1[1]].z, atoms_ptr[idx1[0]].z);
		__m512d x2 = _mm512_set_pd(atoms_ptr[idx2[7]].x, atoms_ptr[idx2[6]].x, atoms_ptr[idx2[5]].x, atoms_ptr[idx2[4]].x,
		                           atoms_ptr[idx2[3]].x, atoms_ptr[idx2[2]].x, atoms_ptr[idx2[1]].x, atoms_ptr[idx2[0]].x);
		__m512d y2 = _mm512_set_pd(atoms_ptr[idx2[7]].y, atoms_ptr[idx2[6]].y, atoms_ptr[idx2[5]].y, atoms_ptr[idx2[4]].y,
		                           atoms_ptr[idx2[3]].y, atoms_ptr[idx2[2]].y, atoms_ptr[idx2[1]].y, atoms_ptr[idx2[0]].y);
		__m512d z2 = _mm512_set_pd(atoms_ptr[idx2[7]].z, atoms_ptr[idx2[6]].z, atoms_ptr[idx2[5]].z, atoms_ptr[idx2[4]].z,
		                           atoms_ptr[idx2[3]].z, atoms_ptr[idx2[2]].z, atoms_ptr[idx2[1]].z, atoms_ptr[idx2[0]].z);
		
		__m512d dx = _mm512_sub_pd(x1, x2);
		__m512d dy = _mm512_sub_pd(y1, y2);
		__m512d dz = _mm512_sub_pd(z1, z2);
		__m512d dx2 = _mm512_mul_pd(dx, dx);
		__m512d dy2 = _mm512_mul_pd(dy, dy);
		__m512d dz2 = _mm512_mul_pd(dz, dz);
		__m512d r2_vec = _mm512_add_pd(_mm512_add_pd(dx2, dy2), dz2);
		
		__m512d r_vec = _mm512_sqrt_pd(r2_vec);
		__m512d one = _mm512_set1_pd(1.0);
		__m512d cutoff = _mm512_set1_pd(9.0);
		__m512d inv_r2_vec = _mm512_div_pd(one, r2_vec);
		
		__m512d vdwRadius1_vec = _mm512_set_pd(pair_eef1[i+7].vdwRadius1, pair_eef1[i+6].vdwRadius1, pair_eef1[i+5].vdwRadius1, pair_eef1[i+4].vdwRadius1,
		                                       pair_eef1[i+3].vdwRadius1, pair_eef1[i+2].vdwRadius1, pair_eef1[i+1].vdwRadius1, pair_eef1[i+0].vdwRadius1);
		__m512d lambda1_vec = _mm512_set_pd(pair_eef1[i+7].lambda1, pair_eef1[i+6].lambda1, pair_eef1[i+5].lambda1, pair_eef1[i+4].lambda1,
		                                    pair_eef1[i+3].lambda1, pair_eef1[i+2].lambda1, pair_eef1[i+1].lambda1, pair_eef1[i+0].lambda1);
		__m512d alpha1_vec = _mm512_set_pd(pair_eef1[i+7].alpha1, pair_eef1[i+6].alpha1, pair_eef1[i+5].alpha1, pair_eef1[i+4].alpha1,
		                                   pair_eef1[i+3].alpha1, pair_eef1[i+2].alpha1, pair_eef1[i+1].alpha1, pair_eef1[i+0].alpha1);
		__m512d vdwRadius2_vec = _mm512_set_pd(pair_eef1[i+7].vdwRadius2, pair_eef1[i+6].vdwRadius2, pair_eef1[i+5].vdwRadius2, pair_eef1[i+4].vdwRadius2,
		                                       pair_eef1[i+3].vdwRadius2, pair_eef1[i+2].vdwRadius2, pair_eef1[i+1].vdwRadius2, pair_eef1[i+0].vdwRadius2);
		__m512d lambda2_vec = _mm512_set_pd(pair_eef1[i+7].lambda2, pair_eef1[i+6].lambda2, pair_eef1[i+5].lambda2, pair_eef1[i+4].lambda2,
		                                    pair_eef1[i+3].lambda2, pair_eef1[i+2].lambda2, pair_eef1[i+1].lambda2, pair_eef1[i+0].lambda2);
		__m512d alpha2_vec = _mm512_set_pd(pair_eef1[i+7].alpha2, pair_eef1[i+6].alpha2, pair_eef1[i+5].alpha2, pair_eef1[i+4].alpha2,
		                                   pair_eef1[i+3].alpha2, pair_eef1[i+2].alpha2, pair_eef1[i+1].alpha2, pair_eef1[i+0].alpha2);
		
		__mmask8 cutoff_mask = _mm512_cmp_pd_mask(r_vec, cutoff, _CMP_LE_OQ);
		__m512d Xij_vec = _mm512_div_pd(_mm512_sub_pd(r_vec, vdwRadius1_vec), lambda1_vec);
		__m512d Xji_vec = _mm512_div_pd(_mm512_sub_pd(r_vec, vdwRadius2_vec), lambda2_vec);
		
		__m512d neg_Xij2_vec = _mm512_mul_pd(_mm512_set1_pd(-1.0), _mm512_mul_pd(Xij_vec, Xij_vec));
		__m512d neg_Xji2_vec = _mm512_mul_pd(_mm512_set1_pd(-1.0), _mm512_mul_pd(Xji_vec, Xji_vec));
		
		double exp_limit_val = 20.0;
		__m512d exp_limit = _mm512_set1_pd(exp_limit_val);
		__m512d x_ij = _mm512_max_pd(neg_Xij2_vec, _mm512_set1_pd(-exp_limit_val));
		__m512d x_ji = _mm512_max_pd(neg_Xji2_vec, _mm512_set1_pd(-exp_limit_val));
		
		__m512d x2_ij = _mm512_mul_pd(x_ij, x_ij);
		__m512d x3_ij = _mm512_mul_pd(x2_ij, x_ij);
		__m512d x4_ij = _mm512_mul_pd(x3_ij, x_ij);
		__m512d x5_ij = _mm512_mul_pd(x4_ij, x_ij);
		__m512d exp_Xij2_vec = _mm512_fmadd_pd(x5_ij, _mm512_set1_pd(1.0/120.0),
			_mm512_fmadd_pd(x4_ij, _mm512_set1_pd(1.0/24.0),
				_mm512_fmadd_pd(x3_ij, _mm512_set1_pd(1.0/6.0),
					_mm512_fmadd_pd(x2_ij, _mm512_set1_pd(0.5),
						_mm512_add_pd(x_ij, _mm512_set1_pd(1.0))))));
		
		__m512d x2_ji = _mm512_mul_pd(x_ji, x_ji);
		__m512d x3_ji = _mm512_mul_pd(x2_ji, x_ji);
		__m512d x4_ji = _mm512_mul_pd(x3_ji, x_ji);
		__m512d x5_ji = _mm512_mul_pd(x4_ji, x_ji);
		__m512d exp_Xji2_vec = _mm512_fmadd_pd(x5_ji, _mm512_set1_pd(1.0/120.0),
			_mm512_fmadd_pd(x4_ji, _mm512_set1_pd(1.0/24.0),
				_mm512_fmadd_pd(x3_ji, _mm512_set1_pd(1.0/6.0),
					_mm512_fmadd_pd(x2_ji, _mm512_set1_pd(0.5),
						_mm512_add_pd(x_ji, _mm512_set1_pd(1.0))))));
		
		__m512d energy_vec = _mm512_mul_pd(_mm512_set1_pd(-1.0),
			_mm512_mul_pd(inv_r2_vec,
				_mm512_add_pd(_mm512_mul_pd(alpha1_vec, exp_Xij2_vec),
					_mm512_mul_pd(alpha2_vec, exp_Xji2_vec))));
		energy_vec = _mm512_maskz_mul_pd(cutoff_mask, energy_vec, _mm512_set1_pd(1.0));
		
		alignas(64) double energy_array[8];
		_mm512_store_pd(energy_array, energy_vec);
		energy += energy_array[0] + energy_array[1] + energy_array[2] + energy_array[3] +
		          energy_array[4] + energy_array[5] + energy_array[6] + energy_array[7];
	}
	
	for (; i < num_eef1; i++) {
		Real3<double> atom1 = atoms[pair_eef1[i].atomi1];
		Real3<double> atom2 = atoms[pair_eef1[i].atomi2];
		double r2 = distance_sq(atom1, atom2);
		assert(!::std::isnan(r2));
		double r = ::std::sqrt(r2);
		energy += pair_eef1[i].calc(r, r2);
	}

	return energy;
}
#endif // USE_AVX512

#endif // USE_SIMD
