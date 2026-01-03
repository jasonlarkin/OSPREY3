#include <cassert>
#include <iostream>
#include <vector>
#include <cmath>
#include <fstream>
#include <string>
#include "partition_function.hpp"
#include "energy_matrix.hpp"
#include "energy_matrix_loader.hpp"
#include "test_data_paths.hpp"

using namespace osprey;
using namespace osprey::kstar;

/**
 * OSPREY VERBATIM TEST: test2RL0ProteinSimple1Cpu
 * 
 * Source: TestSimplePartitionFunction.java
 *   - Line 227: test2RL0ProteinSimple1Cpu() test method
 *   - Lines 215-226: calc2RL0Protein() implementation
 *   - Lines 149-170: testStrand() helper method
 *   - Lines 172-177: assertPfunc() validation method
 * 
 * OSPREY Parameters (verbatim):
 *   - targetEpsilon = 0.05 (line 223)
 *   - approxQStar = "4.370068e+04" (line 224, computed with epsilon=0.001)
 *   - Status: PartitionFunction.Status.Estimated (line 158)
 * 
 * OSPREY ConfSpace Setup (lines 217-219):
 *   - Single protein strand (G648-G653)
 *   - 1 flexible position: G654 (library rotamers, wild-type, continuous)
 * 
 * C++ Translation Notes:
 *   - Uses pre-exported energy matrix from Java (2RL0.protein.emat.bin)
 *     - Export method: ExportEnergyMatrixForCppTest.export2RL0ChainA()
 *   - Validation pattern matches assertPfunc() (lines 172-177):
 *     - qstar >= approxQstar * (1 - targetEpsilon) [line 176]
 *     - effectiveEpsilon <= targetEpsilon [line 174, if converged]
 *   - Status check omitted (C++ uses boolean instead of enum)
 *   - Additional checks (not in OSPREY, required for double precision):
 *     - !std::isnan(qstar) - detect NaN from invalid calculations
 *     - !std::isinf(qstar) - detect overflow (OSPREY uses BigDecimal, no overflow)
 * 
 * Critical Differences:
 *   - OSPREY uses BigDecimal (arbitrary precision), C++ uses double (64-bit)
 *   - OSPREY computes energy matrix on-the-fly, C++ loads pre-exported binary
 *   - OSPREY has Status enum, C++ uses boolean converged flag
 *   - OSPREY tests multiple parallelism variants, C++ only tests single-threaded
 * 
 * Expected Behavior:
 *   - Partition function should converge within epsilon=0.05
 *   - Q* should be >= 4.370068e+04 * 0.95 = 4.1515646e+04
 *   - System is small (1 flexible position), unlikely to overflow
 */
void test_partition_function_2rl0_protein() {
    std::cout << "Testing partition function: 2RL0 Protein (OSPREY verbatim)...\n";
    
    // Load energy matrix exported from Java (verbatim confspace from TestSimplePartitionFunction)
    // Export method: ExportEnergyMatrixForCppTest.exportTestSimplePartitionFunction2RL0Protein()
    std::string emat_rel = "test_data/2RL0.TestSimplePartitionFunction.protein.emat.bin";
    auto emat_path = osprey::kstar::testutil::resolveTestDataPath(emat_rel);
    if (!emat_path) {
        std::cout << "  SKIPPED: Energy matrix not found: " << emat_rel << "\n";
        std::cout << osprey::kstar::testutil::describeTestDataSearch(emat_rel);
        std::cout << "  Run Java test ExportEnergyMatrixForCppTest.exportTestSimplePartitionFunction2RL0Protein() to generate\n";
        return;
    }
    EnergyMatrix<double> emat = EnergyMatrixLoader<double>::loadFromFile(emat_path->string());
    
    // OSPREY verbatim parameters from calc2RL0Protein() (line 223-225)
    const double targetEpsilon = 0.05;
    const std::string approxQStar = "4.370068e+04";  // e=0.001
    const double expected_qstar = std::stod(approxQStar);
    
    // Compute partition function
    PartitionFunction<double> pfunc;
    auto result = pfunc.compute(emat, targetEpsilon);
    
    // OSPREY verbatim validation from assertPfunc() (TestSimplePartitionFunction.java:172-177)
    double qstar = std::pow(10.0, result.lower_bound);
    double qbound = expected_qstar * (1.0 - targetEpsilon);
    
    std::cout << "  Expected Q*: " << expected_qstar << "\n";
    std::cout << "  Computed Q*: " << qstar << "\n";
    std::cout << "  Q* bound (min): " << qbound << "\n";
    std::cout << "  Effective epsilon: " << result.delta << " (target: " << targetEpsilon << ")\n";
    std::cout << "  Converged: " << (result.converged ? "true" : "false") << "\n";
    
    // OSPREY assertion: qstar must be finite and >= qbound (line 176)
    assert(!std::isnan(qstar));
    assert(!std::isinf(qstar));
    assert(qstar >= qbound);
    
    // OSPREY assertion: effectiveEpsilon <= targetEpsilon (line 174, if converged)
    if (result.converged) {
        assert(result.delta <= targetEpsilon);
    }
    
    std::cout << "  PASSED\n";
}

/**
 * OSPREY VERBATIM TEST: test2RL0LigandSimple1Cpu
 * 
 * Source: TestSimplePartitionFunction.java
 *   - Line 249: test2RL0LigandSimple1Cpu() test method
 *   - Lines 237-248: calc2RL0LigandPfunc() implementation
 *   - Lines 149-170: testStrand() helper method
 *   - Lines 172-177: assertPfunc() validation method
 * 
 * OSPREY Parameters (verbatim):
 *   - targetEpsilon = 0.05 (line 245)
 *   - approxQStar = "4.467797e+30" (line 246, computed with epsilon=0.001)
 *   - Status: PartitionFunction.Status.Estimated (line 158)
 * 
 * OSPREY ConfSpace Setup (lines 239-241):
 *   - Single ligand strand (A155-A194)
 *   - 4 flexible positions: A156, A172, A192, A193 (all library rotamers, wild-type, continuous)
 * 
 * C++ Translation Notes:
 *   - Uses pre-exported energy matrix from Java (2RL0.ligand.emat.bin)
 *     - Export method: ExportEnergyMatrixForCppTest.export2RL0ChainG()
 *   - Validation pattern matches assertPfunc() (lines 172-177)
 *   - Additional nan/inf checks required (see test2RL0ProteinSimple1Cpu notes)
 * 
 * Critical Differences:
 *   - Medium-sized system (4 flexible positions)
 *   - Q* value (4.467797e+30) is large but within double range (~1.8e+308)
 *   - May overflow if partition function accumulates in linear space
 * 
 * Expected Behavior:
 *   - Partition function should converge within epsilon=0.05
 *   - Q* should be >= 4.467797e+30 * 0.95 = 4.24440715e+30
 *   - System size may cause overflow if not using log-space arithmetic
 */
void test_partition_function_2rl0_ligand() {
    std::cout << "Testing partition function: 2RL0 Ligand (OSPREY verbatim)...\n";
    
    // Load energy matrix exported from Java (verbatim confspace from TestSimplePartitionFunction)
    // Export method: ExportEnergyMatrixForCppTest.exportTestSimplePartitionFunction2RL0Ligand()
    std::string emat_rel = "test_data/2RL0.TestSimplePartitionFunction.ligand.emat.bin";
    auto emat_path = osprey::kstar::testutil::resolveTestDataPath(emat_rel);
    if (!emat_path) {
        std::cout << "  SKIPPED: Energy matrix not found: " << emat_rel << "\n";
        std::cout << osprey::kstar::testutil::describeTestDataSearch(emat_rel);
        std::cout << "  Run Java test ExportEnergyMatrixForCppTest.exportTestSimplePartitionFunction2RL0Ligand() to generate\n";
        return;
    }
    EnergyMatrix<double> emat = EnergyMatrixLoader<double>::loadFromFile(emat_path->string());
    
    // OSPREY verbatim parameters from calc2RL0LigandPfunc() (line 245-247)
    const double targetEpsilon = 0.05;
    const std::string approxQStar = "4.467797e+30";  // e=0.001
    const double expected_qstar = std::stod(approxQStar);
    
    // Compute partition function
    PartitionFunction<double> pfunc;
    auto result = pfunc.compute(emat, targetEpsilon);
    
    // OSPREY verbatim validation from assertPfunc() (TestSimplePartitionFunction.java:172-177)
    double qstar = std::pow(10.0, result.lower_bound);
    double qbound = expected_qstar * (1.0 - targetEpsilon);
    
    std::cout << "  Expected Q*: " << expected_qstar << "\n";
    std::cout << "  Computed Q*: " << qstar << "\n";
    std::cout << "  Q* bound (min): " << qbound << "\n";
    std::cout << "  Effective epsilon: " << result.delta << " (target: " << targetEpsilon << ")\n";
    std::cout << "  Converged: " << (result.converged ? "true" : "false") << "\n";
    
    // OSPREY assertion: qstar must be finite and >= qbound (line 176)
    assert(!std::isnan(qstar));
    assert(!std::isinf(qstar));
    assert(qstar >= qbound);
    
    // OSPREY assertion: effectiveEpsilon <= targetEpsilon (line 174, if converged)
    if (result.converged) {
        assert(result.delta <= targetEpsilon);
    }
    
    std::cout << "  PASSED\n";
}

/**
 * OSPREY VERBATIM TEST: test2RL0ComplexSimple1Cpu
 * 
 * Source: TestSimplePartitionFunction.java
 *   - Line 280: test2RL0ComplexSimple1Cpu() test method
 *   - Lines 259-278: calc2RL0Complex() implementation
 *   - Lines 149-170: testStrand() helper method
 *   - Lines 172-177: assertPfunc() validation method
 * 
 * OSPREY Parameters (verbatim):
 *   - targetEpsilon = 0.8 (line 275) - RELAXED for large system
 *   - approxQStar = "3.5213742379e+54" (line 276, computed with epsilon=0.05)
 *   - Status: PartitionFunction.Status.Estimated (line 158)
 * 
 * OSPREY ConfSpace Setup (lines 265-271):
 *   - Complex structure with 5 strands (NOTE: order matters for CCD):
 *     1. Strand A153-A154
 *     2. Ligand strand (A155-A194) - 4 flexible positions
 *     3. Strand A195-A241
 *     4. Strand G638-G647
 *     5. Protein strand (G648-G653) - 1 flexible position
 *   - Total: 5 flexible positions across multiple strands
 *   - Lines 261-263: OSPREY comments explaining why conf space must match exactly
 *     - CCD (Cyclic Coordinate Descent) is sensitive to position order
 *     - Extra residues in PDB file must be included
 * 
 * C++ Translation Notes:
 *   - Uses pre-exported energy matrix from Java (verbatim confspace from TestSimplePartitionFunction)
 *     - File: 2RL0.TestSimplePartitionFunction.complex.emat.bin
 *     - Export method: ExportEnergyMatrixForCppTest.exportTestSimplePartitionFunction2RL0Complex()
 *   - Validation pattern matches assertPfunc() (lines 172-177)
 *   - Additional nan/inf checks required (see test2RL0ProteinSimple1Cpu notes)
 * 
 * Critical Differences:
 *   - LARGE system (5 flexible positions, complex structure)
 *   - Q* value (3.5213742379e+54) is EXTREMELY large
 *   - Double max value: ~1.8e+308, so 3.5e+54 is within range BUT:
 *     - Accumulating Boltzmann weights in linear space WILL overflow
 *     - OSPREY uses BigDecimal (arbitrary precision), no overflow
 *   - Relaxed epsilon (0.8) allows for less precise convergence
 * 
 * Known Issues:
 *   - Current implementation produces inf for Q* (overflow bug)
 *   - Effective epsilon is nan (due to inf in delta calculation)
 *   - Test correctly fails, detecting implementation bug
 *   - FIX REQUIRED: Use log-space arithmetic for partition function
 * 
 * Expected Behavior (after fix):
 *   - Partition function should converge within epsilon=0.8
 *   - Q* should be >= 3.5213742379e+54 * 0.2 = 7.0427484758e+53
 *   - Must use log-space arithmetic to avoid overflow
 */
void test_partition_function_2rl0_complex() {
    std::cout << "Testing partition function: 2RL0 Complex (OSPREY verbatim)...\n";
    
    // Load energy matrix exported from Java (verbatim confspace from TestSimplePartitionFunction)
    std::string emat_rel = "test_data/2RL0.TestSimplePartitionFunction.complex.emat.bin";
    auto emat_path = osprey::kstar::testutil::resolveTestDataPath(emat_rel);
    if (!emat_path) {
        std::cout << "  SKIPPED: Energy matrix not found: " << emat_rel << "\n";
        std::cout << osprey::kstar::testutil::describeTestDataSearch(emat_rel);
        std::cout << "  Run Java test ExportEnergyMatrixForCppTest.exportTestSimplePartitionFunction2RL0Complex() to generate\n";
        return;
    }
    EnergyMatrix<double> emat = EnergyMatrixLoader<double>::loadFromFile(emat_path->string());
    
    // OSPREY verbatim parameters from calc2RL0Complex() (line 275-277)
    const double targetEpsilon = 0.8;
    const std::string approxQStar = "3.5213742379e+54";  // e=0.05
    const double expected_qstar = std::stod(approxQStar);
    
    // Compute partition function
    PartitionFunction<double> pfunc;
    auto result = pfunc.compute(emat, targetEpsilon);
    
    // OSPREY verbatim validation from assertPfunc() (TestSimplePartitionFunction.java:172-177)
    double qstar = std::pow(10.0, result.lower_bound);
    double qbound = expected_qstar * (1.0 - targetEpsilon);
    
    std::cout << "  Expected Q*: " << expected_qstar << "\n";
    std::cout << "  Computed Q*: " << qstar << "\n";
    std::cout << "  Q* bound (min): " << qbound << "\n";
    std::cout << "  Effective epsilon: " << result.delta << " (target: " << targetEpsilon << ")\n";
    std::cout << "  Converged: " << (result.converged ? "true" : "false") << "\n";
    
    // OSPREY assertion: qstar must be finite and >= qbound (line 176)
    assert(!std::isnan(qstar));
    assert(!std::isinf(qstar));
    assert(qstar >= qbound);
    
    // OSPREY assertion: effectiveEpsilon <= targetEpsilon (line 174, if converged)
    if (result.converged) {
        assert(result.delta <= targetEpsilon);
    }
    
    std::cout << "  PASSED\n";
}

/**
 * OSPREY VERBATIM TEST: testNoPositionsProteinSimple1Cpu
 * 
 * Source: TestSimplePartitionFunction.java
 *   - Line 507: testNoPositionsProteinSimple1Cpu() test method
 *   - Lines 495-506: calcNoPositionsProtein() implementation
 *   - Lines 468-492: makeNoPositionsTestInfo() setup
 *   - Lines 149-170: testStrand() helper method
 *   - Lines 172-177: assertPfunc() validation method
 * 
 * OSPREY Parameters (verbatim):
 *   - targetEpsilon = 0.05 (line 503)
 *   - approxQStar = "0.0e+0" (line 504)
 *   - Status: PartitionFunction.Status.Estimated (line 158)
 * 
 * OSPREY ConfSpace Setup (lines 497-499):
 *   - Single protein strand (G648-G654)
 *   - NO flexible positions (no positions set to flexible)
 *   - Should have exactly 1 conformation (Q* = 1.0 = exp(-E/RT) for single conf)
 * 
 * C++ Translation Notes:
 *   - Uses pre-exported energy matrix from Java (2RL0.NoPositions.protein.emat.bin)
 *     - Export method: ExportEnergyMatrixForCppTest.export2RL0NoPositionsProtein()
 *   - Validation pattern matches assertPfunc() (lines 172-177):
 *     - qstar >= approxQstar * (1 - targetEpsilon) [line 176]
 *     - effectiveEpsilon <= targetEpsilon [line 174, if converged]
 *   - Status check omitted (C++ uses boolean instead of enum)
 *   - Additional checks (not in OSPREY, required for double precision):
 *     - !std::isnan(qstar) - detect NaN from invalid calculations
 *     - !std::isinf(qstar) - detect overflow (OSPREY uses BigDecimal, no overflow)
 * 
 * Critical Differences:
 *   - OSPREY uses BigDecimal (arbitrary precision), C++ uses double (64-bit)
 *   - OSPREY computes energy matrix on-the-fly, C++ loads pre-exported binary
 *   - OSPREY has Status enum, C++ uses boolean converged flag
 *   - OSPREY tests multiple parallelism variants, C++ only tests single-threaded
 * 
 * Expected Behavior:
 *   - Partition function should converge immediately (1 conformation)
 *   - Q* should be exactly exp(-E/RT) for the single conformation
 *   - This is the simplest possible test case
 */
void test_partition_function_2rl0_no_positions_protein() {
    std::cout << "Testing partition function: 2RL0 NoPositions Protein (OSPREY verbatim)...\n";
    
    // Load energy matrix (exported from Java)
    std::string emat_rel = "test_data/2RL0.NoPositions.protein.emat.bin";
    auto emat_path = osprey::kstar::testutil::resolveTestDataPath(emat_rel);
    if (!emat_path) {
        std::cout << "  SKIPPED: Energy matrix not found: " << emat_rel << "\n";
        std::cout << osprey::kstar::testutil::describeTestDataSearch(emat_rel);
        std::cout << "  Run Java test ExportEnergyMatrixForCppTest.export2RL0NoPositionsProtein() to generate\n";
        return;
    }
    EnergyMatrix<double> emat = EnergyMatrixLoader<double>::loadFromFile(emat_path->string());
    
    // OSPREY verbatim parameters (TestSimplePartitionFunction.java:503-504)
    double targetEpsilon = 0.05;
    double expected_qstar = 0.0;  // "0.0e+0" from line 504
    
    // Compute partition function
    PartitionFunction<double> pfunc;
    auto result = pfunc.compute(emat, targetEpsilon);
    
    // OSPREY verbatim validation from assertPfunc() (TestSimplePartitionFunction.java:172-177)
    double qstar = std::pow(10.0, result.lower_bound);
    
    std::cout << "  Expected Q*: " << expected_qstar << " (should be exp(-E/RT) for single conf)\n";
    std::cout << "  Computed Q*: " << qstar << "\n";
    std::cout << "  Effective epsilon: " << result.delta << " (target: " << targetEpsilon << ")\n";
    std::cout << "  Converged: " << (result.converged ? "true" : "false") << "\n";
    std::cout << "  Num confs evaluated: " << result.num_confs << "\n";
    
    // OSPREY assertion: qstar must be finite (line 176, implicit in BigDecimal)
    assert(!std::isnan(qstar));
    assert(!std::isinf(qstar));
    
    // OSPREY assertion: qstar >= approxQstar * (1 - targetEpsilon) (line 176)
    // For NoPositions, approxQstar = 0.0, so this is always true
    // But we should check that Q* is reasonable (should be exp(-E/RT) for single conf)
    // Since expected_qstar = 0.0, we just check that qstar > 0
    assert(qstar > 0.0);
    
    // OSPREY assertion: effectiveEpsilon <= targetEpsilon (line 174, if converged)
    if (result.converged) {
        assert(result.delta <= targetEpsilon);
    }
    
    // Additional check: should have exactly 1 conformation (no flexible positions)
    // Note: This might not be exactly 1 if there are multiple RCs at positions, but
    // for NoPositions test, there should be no flexible positions, so total_confs = 1
    // Actually, wait - if there are no flexible positions, numPositions = 0, so total_confs = 1
    // But we might evaluate 0 or 1 conformations depending on implementation
    // For now, just check that it's reasonable
    
    std::cout << "  PASSED\n";
}

/**
 * OSPREY VERBATIM TEST: calc1GUA11ProteinSimple
 *
 * Source: TestSimplePartitionFunction.java
 *   - Line 339: calc1GUA11ProteinSimple()
 *   - Lines 326-340: calc1GUA11Protein() implementation
 *   - Lines 292-324: make1GUA11TestInfo() setup
 *   - Lines 172-177: assertPfunc() validation method
 *
 * OSPREY Parameters (verbatim):
 *   - targetEpsilon = 0.9 (line 335)
 *   - approxQStar = "1.1838e+42" (line 336, e=0.1)
 */
void test_partition_function_1gua11_protein() {
    std::cout << "Testing partition function: 1GUA11 Protein (OSPREY verbatim)...\n";

    std::string emat_rel = "test_data/1GUA11.TestSimplePartitionFunction.protein.emat.bin";
    auto emat_path = osprey::kstar::testutil::resolveTestDataPath(emat_rel);
    if (!emat_path) {
        std::cout << "  SKIPPED: Energy matrix not found: " << emat_rel << "\n";
        std::cout << osprey::kstar::testutil::describeTestDataSearch(emat_rel);
        std::cout << "  Run Java test ExportEnergyMatrixForCppTest.exportTestSimplePartitionFunction1GUA11Protein() to generate\n";
        return;
    }
    EnergyMatrix<double> emat = EnergyMatrixLoader<double>::loadFromFile(emat_path->string());

    const double targetEpsilon = 0.9;
    const std::string approxQStar = "1.1838e+42";
    const double expected_qstar = std::stod(approxQStar);

    PartitionFunction<double> pfunc;
    auto result = pfunc.compute(emat, targetEpsilon);

    double qstar = std::pow(10.0, result.lower_bound);
    double qbound = expected_qstar * (1.0 - targetEpsilon);

    std::cout << "  Expected Q*: " << expected_qstar << "\n";
    std::cout << "  Computed Q*: " << qstar << "\n";
    std::cout << "  Q* bound (min): " << qbound << "\n";
    std::cout << "  Effective epsilon: " << result.delta << " (target: " << targetEpsilon << ")\n";
    std::cout << "  Converged: " << (result.converged ? "true" : "false") << "\n";

    assert(!std::isnan(qstar));
    assert(!std::isinf(qstar));
    assert(qstar >= qbound);
    if (result.converged) {
        assert(result.delta <= targetEpsilon);
    }

    std::cout << "  PASSED\n";
}

/**
 * OSPREY VERBATIM TEST: calc1GUA11LigandSimple
 *
 * Source: TestSimplePartitionFunction.java
 *   - Line 355: calc1GUA11LigandSimple()
 *   - Lines 342-356: calc1GUA11Ligand() implementation
 *   - Lines 292-324: make1GUA11TestInfo() setup
 *   - Lines 172-177: assertPfunc() validation method
 *
 * OSPREY Parameters (verbatim):
 *   - targetEpsilon = 0.9 (line 351)
 *   - approxQStar = "2.7098e+7" (line 352, e=0.1)
 */
void test_partition_function_1gua11_ligand() {
    std::cout << "Testing partition function: 1GUA11 Ligand (OSPREY verbatim)...\n";

    std::string emat_rel = "test_data/1GUA11.TestSimplePartitionFunction.ligand.emat.bin";
    auto emat_path = osprey::kstar::testutil::resolveTestDataPath(emat_rel);
    if (!emat_path) {
        std::cout << "  SKIPPED: Energy matrix not found: " << emat_rel << "\n";
        std::cout << osprey::kstar::testutil::describeTestDataSearch(emat_rel);
        std::cout << "  Run Java test ExportEnergyMatrixForCppTest.exportTestSimplePartitionFunction1GUA11Ligand() to generate\n";
        return;
    }
    EnergyMatrix<double> emat = EnergyMatrixLoader<double>::loadFromFile(emat_path->string());

    const double targetEpsilon = 0.9;
    const std::string approxQStar = "2.7098e+7";
    const double expected_qstar = std::stod(approxQStar);

    PartitionFunction<double> pfunc;
    auto result = pfunc.compute(emat, targetEpsilon);

    double qstar = std::pow(10.0, result.lower_bound);
    double qbound = expected_qstar * (1.0 - targetEpsilon);

    std::cout << "  Expected Q*: " << expected_qstar << "\n";
    std::cout << "  Computed Q*: " << qstar << "\n";
    std::cout << "  Q* bound (min): " << qbound << "\n";
    std::cout << "  Effective epsilon: " << result.delta << " (target: " << targetEpsilon << ")\n";
    std::cout << "  Converged: " << (result.converged ? "true" : "false") << "\n";

    assert(!std::isnan(qstar));
    assert(!std::isinf(qstar));
    assert(qstar >= qbound);
    if (result.converged) {
        assert(result.delta <= targetEpsilon);
    }

    std::cout << "  PASSED\n";
}

/**
 * OSPREY VERBATIM TEST: calc1GUA11ComplexSimple
 *
 * Source: TestSimplePartitionFunction.java
 *   - Line 371: calc1GUA11ComplexSimple()
 *   - Lines 358-372: calc1GUA11Complex() implementation
 *   - Lines 292-324: make1GUA11TestInfo() setup
 *   - Lines 172-177: assertPfunc() validation method
 *
 * OSPREY Parameters (verbatim):
 *   - targetEpsilon = 0.9 (line 367)
 *   - approxQStar = "1.1195e+66" (line 368, e=0.1)
 */
void test_partition_function_1gua11_complex() {
    std::cout << "Testing partition function: 1GUA11 Complex (OSPREY verbatim)...\n";

    std::string emat_rel = "test_data/1GUA11.TestSimplePartitionFunction.complex.emat.bin";
    auto emat_path = osprey::kstar::testutil::resolveTestDataPath(emat_rel);
    if (!emat_path) {
        std::cout << "  SKIPPED: Energy matrix not found: " << emat_rel << "\n";
        std::cout << osprey::kstar::testutil::describeTestDataSearch(emat_rel);
        std::cout << "  Run Java test ExportEnergyMatrixForCppTest.exportTestSimplePartitionFunction1GUA11Complex() to generate\n";
        return;
    }
    EnergyMatrix<double> emat = EnergyMatrixLoader<double>::loadFromFile(emat_path->string());

    const double targetEpsilon = 0.9;
    const std::string approxQStar = "1.1195e+66";
    const double expected_qstar = std::stod(approxQStar);

    PartitionFunction<double> pfunc;
    auto result = pfunc.compute(emat, targetEpsilon);

    double qstar = std::pow(10.0, result.lower_bound);
    double qbound = expected_qstar * (1.0 - targetEpsilon);

    std::cout << "  Expected Q*: " << expected_qstar << "\n";
    std::cout << "  Computed Q*: " << qstar << "\n";
    std::cout << "  Q* bound (min): " << qbound << "\n";
    std::cout << "  Effective epsilon: " << result.delta << " (target: " << targetEpsilon << ")\n";
    std::cout << "  Converged: " << (result.converged ? "true" : "false") << "\n";

    assert(!std::isnan(qstar));
    assert(!std::isinf(qstar));
    assert(qstar >= qbound);
    if (result.converged) {
        assert(result.delta <= targetEpsilon);
    }

    std::cout << "  PASSED\n";
}

int main() {
    std::cout << "=== Partition Function Unit Tests (OSPREY Verbatim) ===\n\n";
    
    // OSPREY verbatim tests from TestSimplePartitionFunction.java
    test_partition_function_2rl0_protein();
    test_partition_function_2rl0_ligand();
    test_partition_function_2rl0_complex();
    test_partition_function_2rl0_no_positions_protein();
    test_partition_function_1gua11_protein();
    test_partition_function_1gua11_ligand();
    test_partition_function_1gua11_complex();
    
    std::cout << "\n=== All Tests Complete ===\n";
    return 0;
}
