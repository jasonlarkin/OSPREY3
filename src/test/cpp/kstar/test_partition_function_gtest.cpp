#include <gtest/gtest.h>

#include <cmath>
#include <fstream>
#include <optional>
#include <string>

#include "partition_function.hpp"
#include "energy_matrix.hpp"
#include "energy_matrix_loader.hpp"
#include "test_data_paths.hpp"

using namespace osprey::kstar;

static std::optional<EnergyMatrix<double>> tryLoadEmat(const std::string& emat_path) {
    auto resolved = osprey::kstar::testutil::resolveTestDataPath(emat_path);
    if (!resolved) {
        return std::nullopt;
    }
    std::ifstream file(resolved->string());
    if (!file.good()) {
        return std::nullopt;
    }
    file.close();
    return EnergyMatrixLoader<double>::loadFromFile(resolved->string());
}

static void assertPfuncVerbatim(
    const PartitionFunctionResult<double>& result,
    double target_epsilon,
    double expected_qstar,
    const char* label
) {
    const double qstar = std::pow(10.0, result.lower_bound);
    const double qbound = expected_qstar * (1.0 - target_epsilon);

    SCOPED_TRACE(label);
    SCOPED_TRACE(::testing::Message() << "expected_qstar=" << expected_qstar
                                      << " target_epsilon=" << target_epsilon
                                      << " qstar=" << qstar
                                      << " qbound=" << qbound
                                      << " delta=" << result.delta
                                      << " converged=" << result.converged
                                      << " num_confs=" << result.num_confs);

    ASSERT_FALSE(std::isnan(qstar));
    ASSERT_FALSE(std::isinf(qstar));
    ASSERT_GE(qstar, qbound);

    if (result.converged) {
        ASSERT_LE(result.delta, target_epsilon);
    }
}

static void runPfuncVerbatimOrSkip(
    const char* emat_path,
    double targetEpsilon,
    double expectedQStar,
    const char* label,
    PartitionFunctionMethod method
) {
    auto ematOpt = tryLoadEmat(emat_path);
    if (!ematOpt) {
        GTEST_SKIP() << "Energy matrix not found: " << emat_path << "\n"
                     << osprey::kstar::testutil::describeTestDataSearch(emat_path);
    }
    auto emat = *std::move(ematOpt);

    PartitionFunction<double> pfunc;
    auto result = pfunc.compute(emat, targetEpsilon, method);
    assertPfuncVerbatim(result, targetEpsilon, expectedQStar, label);
}

// ===================== 2RL0 (TestSimplePartitionFunction) =====================

TEST(PartitionFunction_VERBATIM, RL0_Protein_Simple1Cpu) {
    const std::string emat_path = "test_data/2RL0.TestSimplePartitionFunction.protein.emat.bin";
    auto ematOpt = tryLoadEmat(emat_path);
    if (!ematOpt) {
        GTEST_SKIP() << "Energy matrix not found: " << emat_path
                     << "\nRun Java test ExportEnergyMatrixForCppTest.exportTestSimplePartitionFunction2RL0Protein() to generate";
    }
    auto emat = *std::move(ematOpt);

    const double targetEpsilon = 0.05;
    const double expectedQStar = 4.370068e+04;

    PartitionFunction<double> pfunc;
    auto result = pfunc.compute(emat, targetEpsilon);
    assertPfuncVerbatim(result, targetEpsilon, expectedQStar, "2RL0 Protein");
}

TEST(PartitionFunction_VERBATIM, RL0_Protein_Simple2Cpus) {
    // Java: test2RL0ProteinSimple2Cpus() uses Parallelism.make(2, 0, 0)
    // C++: currently single-threaded; EnergyMatrix input matches the Java conf space.
    const std::string emat_path = "test_data/2RL0.TestSimplePartitionFunction.protein.emat.bin";
    auto ematOpt = tryLoadEmat(emat_path);
    if (!ematOpt) {
        GTEST_SKIP() << "Energy matrix not found: " << emat_path
                     << "\nRun Java test ExportEnergyMatrixForCppTest.exportTestSimplePartitionFunction2RL0Protein() to generate";
    }
    auto emat = *std::move(ematOpt);

    const double targetEpsilon = 0.05;
    const double expectedQStar = 4.370068e+04;

    PartitionFunction<double> pfunc;
    auto result = pfunc.compute(emat, targetEpsilon);
    assertPfuncVerbatim(result, targetEpsilon, expectedQStar, "2RL0 Protein (Simple2Cpus)");
}

TEST(PartitionFunction_VERBATIM, RL0_Protein_Simple1GpuStream) {
    GTEST_SKIP() << "VERBATIM placeholder for Java TestSimplePartitionFunction.test2RL0ProteinSimple1GpuStream (GPU not implemented in C++)";
}

TEST(PartitionFunction_VERBATIM, RL0_Protein_Simple4GpuStreams) {
    GTEST_SKIP() << "VERBATIM placeholder for Java TestSimplePartitionFunction.test2RL0ProteinSimple4GpuStreams (GPU not implemented in C++)";
}

TEST(PartitionFunction_VERBATIM, RL0_Protein_GD1Cpu) {
    runPfuncVerbatimOrSkip(
        "test_data/2RL0.TestSimplePartitionFunction.protein.emat.bin",
        0.05,
        4.370068e+04,
        "2RL0 Protein (GD1Cpu)",
        PartitionFunctionMethod::GradientDescent
    );
}

TEST(PartitionFunction_VERBATIM, RL0_Protein_GD2Cpus) {
    runPfuncVerbatimOrSkip(
        "test_data/2RL0.TestSimplePartitionFunction.protein.emat.bin",
        0.05,
        4.370068e+04,
        "2RL0 Protein (GD2Cpus)",
        PartitionFunctionMethod::GradientDescent
    );
}

TEST(PartitionFunction_VERBATIM, RL0_Protein_GD1GpuStream) {
    GTEST_SKIP() << "VERBATIM placeholder for Java TestSimplePartitionFunction.test2RL0ProteinGD1GpuStream (GD + GPU not implemented in C++)";
}

TEST(PartitionFunction_VERBATIM, RL0_Protein_GD4GpuStreams) {
    GTEST_SKIP() << "VERBATIM placeholder for Java TestSimplePartitionFunction.test2RL0ProteinGD4GpuStreams (GD + GPU not implemented in C++)";
}

TEST(PartitionFunction_VERBATIM, RL0_Ligand_Simple1Cpu) {
    const std::string emat_path = "test_data/2RL0.TestSimplePartitionFunction.ligand.emat.bin";
    auto ematOpt = tryLoadEmat(emat_path);
    if (!ematOpt) {
        GTEST_SKIP() << "Energy matrix not found: " << emat_path
                     << "\nRun Java test ExportEnergyMatrixForCppTest.exportTestSimplePartitionFunction2RL0Ligand() to generate";
    }
    auto emat = *std::move(ematOpt);

    const double targetEpsilon = 0.05;
    const double expectedQStar = 4.467797e+30;

    PartitionFunction<double> pfunc;
    auto result = pfunc.compute(emat, targetEpsilon);
    assertPfuncVerbatim(result, targetEpsilon, expectedQStar, "2RL0 Ligand");
}

TEST(PartitionFunction_VERBATIM, RL0_Ligand_Simple2Cpus) {
    // Java: test2RL0LigandSimple2Cpus() uses Parallelism.make(2, 0, 0)
    const std::string emat_path = "test_data/2RL0.TestSimplePartitionFunction.ligand.emat.bin";
    auto ematOpt = tryLoadEmat(emat_path);
    if (!ematOpt) {
        GTEST_SKIP() << "Energy matrix not found: " << emat_path
                     << "\nRun Java test ExportEnergyMatrixForCppTest.exportTestSimplePartitionFunction2RL0Ligand() to generate";
    }
    auto emat = *std::move(ematOpt);

    const double targetEpsilon = 0.05;
    const double expectedQStar = 4.467797e+30;

    PartitionFunction<double> pfunc;
    auto result = pfunc.compute(emat, targetEpsilon);
    assertPfuncVerbatim(result, targetEpsilon, expectedQStar, "2RL0 Ligand (Simple2Cpus)");
}

TEST(PartitionFunction_VERBATIM, RL0_Ligand_Simple1GpuStream) {
    GTEST_SKIP() << "VERBATIM placeholder for Java TestSimplePartitionFunction.test2RL0LigandSimple1GpuStream (GPU not implemented in C++)";
}

TEST(PartitionFunction_VERBATIM, RL0_Ligand_Simple4GpuStreams) {
    GTEST_SKIP() << "VERBATIM placeholder for Java TestSimplePartitionFunction.test2RL0LigandSimple4GpuStreams (GPU not implemented in C++)";
}

TEST(PartitionFunction_VERBATIM, RL0_Ligand_GD1Cpu) {
    runPfuncVerbatimOrSkip(
        "test_data/2RL0.TestSimplePartitionFunction.ligand.emat.bin",
        0.05,
        4.467797e+30,
        "2RL0 Ligand (GD1Cpu)",
        PartitionFunctionMethod::GradientDescent
    );
}

TEST(PartitionFunction_VERBATIM, RL0_Ligand_GD2Cpus) {
    runPfuncVerbatimOrSkip(
        "test_data/2RL0.TestSimplePartitionFunction.ligand.emat.bin",
        0.05,
        4.467797e+30,
        "2RL0 Ligand (GD2Cpus)",
        PartitionFunctionMethod::GradientDescent
    );
}

TEST(PartitionFunction_VERBATIM, RL0_Ligand_GD1GpuStream) {
    GTEST_SKIP() << "VERBATIM placeholder for Java TestSimplePartitionFunction.test2RL0LigandGD1GpuStream (GD + GPU not implemented in C++)";
}

TEST(PartitionFunction_VERBATIM, RL0_Ligand_GD4GpuStreams) {
    GTEST_SKIP() << "VERBATIM placeholder for Java TestSimplePartitionFunction.test2RL0LigandGD4GpuStreams (GD + GPU not implemented in C++)";
}

TEST(PartitionFunction_VERBATIM, RL0_Complex_Simple1Cpu) {
    const std::string emat_path = "test_data/2RL0.TestSimplePartitionFunction.complex.emat.bin";
    auto ematOpt = tryLoadEmat(emat_path);
    if (!ematOpt) {
        GTEST_SKIP() << "Energy matrix not found: " << emat_path
                     << "\nRun Java test ExportEnergyMatrixForCppTest.exportTestSimplePartitionFunction2RL0Complex() to generate";
    }
    auto emat = *std::move(ematOpt);

    const double targetEpsilon = 0.8;
    const double expectedQStar = 3.5213742379e+54;

    PartitionFunction<double> pfunc;
    auto result = pfunc.compute(emat, targetEpsilon);
    assertPfuncVerbatim(result, targetEpsilon, expectedQStar, "2RL0 Complex");
}

TEST(PartitionFunction_VERBATIM, RL0_Complex_Simple2Cpus) {
    // Java: test2RL0ComplexSimple2Cpus() uses Parallelism.make(2, 0, 0)
    const std::string emat_path = "test_data/2RL0.TestSimplePartitionFunction.complex.emat.bin";
    auto ematOpt = tryLoadEmat(emat_path);
    if (!ematOpt) {
        GTEST_SKIP() << "Energy matrix not found: " << emat_path
                     << "\nRun Java test ExportEnergyMatrixForCppTest.exportTestSimplePartitionFunction2RL0Complex() to generate";
    }
    auto emat = *std::move(ematOpt);

    const double targetEpsilon = 0.8;
    const double expectedQStar = 3.5213742379e+54;

    PartitionFunction<double> pfunc;
    auto result = pfunc.compute(emat, targetEpsilon);
    assertPfuncVerbatim(result, targetEpsilon, expectedQStar, "2RL0 Complex (Simple2Cpus)");
}

TEST(PartitionFunction_VERBATIM, RL0_Complex_Simple4Cpus) {
    // Java: test2RL0ComplexSimple4Cpus() uses Parallelism.make(4, 0, 0)
    const std::string emat_path = "test_data/2RL0.TestSimplePartitionFunction.complex.emat.bin";
    auto ematOpt = tryLoadEmat(emat_path);
    if (!ematOpt) {
        GTEST_SKIP() << "Energy matrix not found: " << emat_path
                     << "\nRun Java test ExportEnergyMatrixForCppTest.exportTestSimplePartitionFunction2RL0Complex() to generate";
    }
    auto emat = *std::move(ematOpt);

    const double targetEpsilon = 0.8;
    const double expectedQStar = 3.5213742379e+54;

    PartitionFunction<double> pfunc;
    auto result = pfunc.compute(emat, targetEpsilon);
    assertPfuncVerbatim(result, targetEpsilon, expectedQStar, "2RL0 Complex (Simple4Cpus)");
}

TEST(PartitionFunction_VERBATIM, RL0_Complex_Simple1GpuStream) {
    GTEST_SKIP() << "VERBATIM placeholder for Java TestSimplePartitionFunction.test2RL0ComplexSimple1GpuStream (GPU not implemented in C++)";
}

TEST(PartitionFunction_VERBATIM, RL0_Complex_Simple4GpuStreams) {
    GTEST_SKIP() << "VERBATIM placeholder for Java TestSimplePartitionFunction.test2RL0ComplexSimple4GpuStreams (GPU not implemented in C++)";
}

TEST(PartitionFunction_VERBATIM, RL0_Complex_GD1Cpu) {
    runPfuncVerbatimOrSkip(
        "test_data/2RL0.TestSimplePartitionFunction.complex.emat.bin",
        0.8,
        3.5213742379e+54,
        "2RL0 Complex (GD1Cpu)",
        PartitionFunctionMethod::GradientDescent
    );
}

TEST(PartitionFunction_VERBATIM, RL0_Complex_GD2Cpus) {
    runPfuncVerbatimOrSkip(
        "test_data/2RL0.TestSimplePartitionFunction.complex.emat.bin",
        0.8,
        3.5213742379e+54,
        "2RL0 Complex (GD2Cpus)",
        PartitionFunctionMethod::GradientDescent
    );
}

TEST(PartitionFunction_VERBATIM, RL0_Complex_GD4Cpus) {
    runPfuncVerbatimOrSkip(
        "test_data/2RL0.TestSimplePartitionFunction.complex.emat.bin",
        0.8,
        3.5213742379e+54,
        "2RL0 Complex (GD4Cpus)",
        PartitionFunctionMethod::GradientDescent
    );
}

TEST(PartitionFunction_VERBATIM, RL0_Complex_GD1GpuStream) {
    GTEST_SKIP() << "VERBATIM placeholder for Java TestSimplePartitionFunction.test2RL0ComplexGD1GpuStream (GD + GPU not implemented in C++)";
}

TEST(PartitionFunction_VERBATIM, RL0_Complex_GD4GpuStreams) {
    GTEST_SKIP() << "VERBATIM placeholder for Java TestSimplePartitionFunction.test2RL0ComplexGD4GpuStreams (GD + GPU not implemented in C++)";
}

TEST(PartitionFunction_VERBATIM, RL0_NoPositions_Protein_Simple1Cpu) {
    const std::string emat_path = "test_data/2RL0.NoPositions.protein.emat.bin";
    auto ematOpt = tryLoadEmat(emat_path);
    if (!ematOpt) {
        GTEST_SKIP() << "Energy matrix not found: " << emat_path
                     << "\nRun Java test ExportEnergyMatrixForCppTest.export2RL0NoPositionsProtein() to generate";
    }
    auto emat = *std::move(ematOpt);

    const double targetEpsilon = 0.05;
    const double expectedQStar = 0.0; // "0.0e+0"

    PartitionFunction<double> pfunc;
    auto result = pfunc.compute(emat, targetEpsilon);

    const double qstar = std::pow(10.0, result.lower_bound);
    ASSERT_FALSE(std::isnan(qstar));
    ASSERT_FALSE(std::isinf(qstar));
    ASSERT_GT(qstar, 0.0);
    ASSERT_TRUE(result.converged);
    ASSERT_LE(result.delta, targetEpsilon);

    (void)expectedQStar; // only used for documentation equivalence
}

TEST(PartitionFunction_VERBATIM, RL0_NoPositions_Protein_Simple2Cpus) {
    // Java: testNoPositionsProteinSimple2Cpus() uses Parallelism.make(2, 0, 0)
    const std::string emat_path = "test_data/2RL0.NoPositions.protein.emat.bin";
    auto ematOpt = tryLoadEmat(emat_path);
    if (!ematOpt) {
        GTEST_SKIP() << "Energy matrix not found: " << emat_path
                     << "\nRun Java test ExportEnergyMatrixForCppTest.export2RL0NoPositionsProtein() to generate";
    }
    auto emat = *std::move(ematOpt);

    const double targetEpsilon = 0.05;
    const double expectedQStar = 0.0; // "0.0e+0"

    PartitionFunction<double> pfunc;
    auto result = pfunc.compute(emat, targetEpsilon);

    const double qstar = std::pow(10.0, result.lower_bound);
    ASSERT_FALSE(std::isnan(qstar));
    ASSERT_FALSE(std::isinf(qstar));
    ASSERT_GT(qstar, 0.0);
    ASSERT_TRUE(result.converged);
    ASSERT_LE(result.delta, targetEpsilon);

    (void)expectedQStar;
}

TEST(PartitionFunction_VERBATIM, RL0_NoPositions_Protein_Simple1GpuStream) {
    GTEST_SKIP() << "VERBATIM placeholder for Java TestSimplePartitionFunction.testNoPositionsProteinSimple1GpuStream (GPU not implemented in C++)";
}

TEST(PartitionFunction_VERBATIM, RL0_NoPositions_Protein_Simple4GpuStreams) {
    GTEST_SKIP() << "VERBATIM placeholder for Java TestSimplePartitionFunction.testNoPositionsProteinSimple4GpuStreams (GPU not implemented in C++)";
}

TEST(PartitionFunction_VERBATIM, RL0_NoPositions_Protein_GD1Cpu) {
    const std::string emat_path = "test_data/2RL0.NoPositions.protein.emat.bin";
    auto ematOpt = tryLoadEmat(emat_path);
    if (!ematOpt) {
        GTEST_SKIP() << "Energy matrix not found: " << emat_path
                     << "\nRun Java test ExportEnergyMatrixForCppTest.export2RL0NoPositionsProtein() to generate";
    }
    auto emat = *std::move(ematOpt);

    const double targetEpsilon = 0.05;

    PartitionFunction<double> pfunc;
    auto result = pfunc.compute(emat, targetEpsilon, PartitionFunctionMethod::GradientDescent);

    const double qstar = std::pow(10.0, result.lower_bound);
    ASSERT_FALSE(std::isnan(qstar));
    ASSERT_FALSE(std::isinf(qstar));
    ASSERT_GT(qstar, 0.0);
    ASSERT_TRUE(result.converged);
    ASSERT_LE(result.delta, targetEpsilon);
}

TEST(PartitionFunction_VERBATIM, RL0_NoPositions_Protein_GD2Cpus) {
    // Java: testNoPositionsProteinGD2Cpus() uses Parallelism.make(2, 0, 0)
    const std::string emat_path = "test_data/2RL0.NoPositions.protein.emat.bin";
    auto ematOpt = tryLoadEmat(emat_path);
    if (!ematOpt) {
        GTEST_SKIP() << "Energy matrix not found: " << emat_path
                     << "\nRun Java test ExportEnergyMatrixForCppTest.export2RL0NoPositionsProtein() to generate";
    }
    auto emat = *std::move(ematOpt);

    const double targetEpsilon = 0.05;

    PartitionFunction<double> pfunc;
    auto result = pfunc.compute(emat, targetEpsilon, PartitionFunctionMethod::GradientDescent);

    const double qstar = std::pow(10.0, result.lower_bound);
    ASSERT_FALSE(std::isnan(qstar));
    ASSERT_FALSE(std::isinf(qstar));
    ASSERT_GT(qstar, 0.0);
    ASSERT_TRUE(result.converged);
    ASSERT_LE(result.delta, targetEpsilon);
}

TEST(PartitionFunction_VERBATIM, RL0_NoPositions_Protein_GD1GpuStream) {
    GTEST_SKIP() << "VERBATIM placeholder for Java TestSimplePartitionFunction.testNoPositionsProteinGD1GpuStream (GD + GPU not implemented in C++)";
}

TEST(PartitionFunction_VERBATIM, RL0_NoPositions_Protein_GD4GpuStreams) {
    GTEST_SKIP() << "VERBATIM placeholder for Java TestSimplePartitionFunction.testNoPositionsProteinGD4GpuStreams (GD + GPU not implemented in C++)";
}

// ===================== 1GUA11 (TestSimplePartitionFunction) =====================

TEST(PartitionFunction_VERBATIM, GUA11_Protein_Simple) {
    const std::string emat_path = "test_data/1GUA11.TestSimplePartitionFunction.protein.emat.bin";
    auto ematOpt = tryLoadEmat(emat_path);
    if (!ematOpt) {
        GTEST_SKIP() << "Energy matrix not found: " << emat_path
                     << "\nRun Java test ExportEnergyMatrixForCppTest.exportTestSimplePartitionFunction1GUA11Protein() to generate";
    }
    auto emat = *std::move(ematOpt);

    const double targetEpsilon = 0.9;
    const double expectedQStar = 1.1838e+42;

    PartitionFunction<double> pfunc;
    auto result = pfunc.compute(emat, targetEpsilon);
    assertPfuncVerbatim(result, targetEpsilon, expectedQStar, "1GUA11 Protein");
}

TEST(PartitionFunction_VERBATIM, GUA11_Protein_GD) {
    runPfuncVerbatimOrSkip(
        "test_data/1GUA11.TestSimplePartitionFunction.protein.emat.bin",
        0.9,
        1.1838e+42,
        "1GUA11 Protein (GD)",
        PartitionFunctionMethod::GradientDescent
    );
}

TEST(PartitionFunction_VERBATIM, GUA11_Ligand_Simple) {
    const std::string emat_path = "test_data/1GUA11.TestSimplePartitionFunction.ligand.emat.bin";
    auto ematOpt = tryLoadEmat(emat_path);
    if (!ematOpt) {
        GTEST_SKIP() << "Energy matrix not found: " << emat_path
                     << "\nRun Java test ExportEnergyMatrixForCppTest.exportTestSimplePartitionFunction1GUA11Ligand() to generate";
    }
    auto emat = *std::move(ematOpt);

    const double targetEpsilon = 0.9;
    const double expectedQStar = 2.7098e+7;

    PartitionFunction<double> pfunc;
    auto result = pfunc.compute(emat, targetEpsilon);
    assertPfuncVerbatim(result, targetEpsilon, expectedQStar, "1GUA11 Ligand");
}

TEST(PartitionFunction_VERBATIM, GUA11_Ligand_GD) {
    runPfuncVerbatimOrSkip(
        "test_data/1GUA11.TestSimplePartitionFunction.ligand.emat.bin",
        0.9,
        2.7098e+7,
        "1GUA11 Ligand (GD)",
        PartitionFunctionMethod::GradientDescent
    );
}

TEST(PartitionFunction_VERBATIM, GUA11_Complex_Simple) {
    const std::string emat_path = "test_data/1GUA11.TestSimplePartitionFunction.complex.emat.bin";
    auto ematOpt = tryLoadEmat(emat_path);
    if (!ematOpt) {
        GTEST_SKIP() << "Energy matrix not found: " << emat_path
                     << "\nRun Java test ExportEnergyMatrixForCppTest.exportTestSimplePartitionFunction1GUA11Complex() to generate";
    }
    auto emat = *std::move(ematOpt);

    const double targetEpsilon = 0.9;
    const double expectedQStar = 1.1195e+66;

    PartitionFunction<double> pfunc;
    auto result = pfunc.compute(emat, targetEpsilon);
    assertPfuncVerbatim(result, targetEpsilon, expectedQStar, "1GUA11 Complex");
}

TEST(PartitionFunction_VERBATIM, GUA11_Complex_GD) {
    runPfuncVerbatimOrSkip(
        "test_data/1GUA11.TestSimplePartitionFunction.complex.emat.bin",
        0.9,
        1.1195e+66,
        "1GUA11 Complex (GD)",
        PartitionFunctionMethod::GradientDescent
    );
}

TEST(PartitionFunction_VERBATIM, RL0_Ligand_WithConfDB_GD) {
    GTEST_SKIP() << "VERBATIM placeholder for Java TestSimplePartitionFunction.calcWithConfDBGD (ConfDB not implemented in C++)";
}

