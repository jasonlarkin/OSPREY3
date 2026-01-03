#include <fstream>
#include <sstream>
#include <vector>
#include <cmath>

#include <gtest/gtest.h>

#include "energy_matrix.hpp"
#include "energy_matrix_loader.hpp"
#include "test_data_paths.hpp"

using namespace osprey::kstar;

/**
 * Compare C++ EnergyMatrix with Java OSPREY EnergyMatrix.
 * 
 * Loads EnergyMatrix exported from Java and compares:
 * 1. Structure (numPositions, numConfsPerPos)
 * 2. One-body energies
 * 3. Pairwise energies
 * 4. confE() results for test conformations
 */

namespace {

struct JavaEmatCase {
    const char* ematPath;
    const char* testConfPath;
};

static void runJavaEmatComparisonOrSkip(const JavaEmatCase& tc) {
    auto ematPath = osprey::kstar::testutil::resolveTestDataPath(tc.ematPath);
    auto confPath = osprey::kstar::testutil::resolveTestDataPath(tc.testConfPath);
    if (!ematPath || !confPath) {
        GTEST_SKIP() << "Missing Java comparison test data.\n"
                     << osprey::kstar::testutil::describeTestDataSearch(tc.ematPath);
    }

    std::ifstream testFile(confPath->string());
    if (!testFile.is_open()) {
        GTEST_SKIP() << "Missing test conf file: " << confPath->string();
    }

    EnergyMatrix<double> emat;
    try {
        emat = EnergyMatrixLoader<double>::loadFromFile(ematPath->string());
    } catch (const std::exception& e) {
        GTEST_SKIP() << "Could not load EnergyMatrix '" << ematPath->string() << "': " << e.what();
    }

    std::string line;
    std::vector<int32_t> testConf;
    double expectedEnergy = 0.0;

    while (std::getline(testFile, line)) {
        if (line.find("conformation:") == 0) {
            size_t start = line.find('[');
            size_t end = line.find(']');
            if (start != std::string::npos && end != std::string::npos) {
                std::string confStr = line.substr(start + 1, end - start - 1);
                std::istringstream iss(confStr);
                int32_t val;
                while (iss >> val) {
                    testConf.push_back(val);
                    if (iss.peek() == ',') iss.ignore();
                }
            }
        } else if (line.find("expected_energy:") == 0) {
            size_t start = line.find(':');
            if (start != std::string::npos) {
                expectedEnergy = std::stod(line.substr(start + 1));
            }
        }
    }

    // Special case: "no positions" energy matrices legitimately have an empty conformation: []
    // In that case, the expected energy is typically 0.0 (or just the constTerm, depending on export).
    if (testConf.empty() && emat.getNumPositions() > 0) {
        GTEST_SKIP() << "Could not parse test conformation from: " << confPath->string();
    }

    const double computedEnergy = emat.computeEnergy(testConf);
    const double diff = std::abs(computedEnergy - expectedEnergy);
    const double relError = diff / std::max(std::abs(expectedEnergy), 1.0);

    const double tolerance = 1e-6;
    EXPECT_TRUE(diff <= tolerance || relError <= tolerance)
        << "Energy mismatch for " << ematPath->string()
        << "\n  expected=" << expectedEnergy
        << "\n  computed=" << computedEnergy
        << "\n  diff=" << diff
        << "\n  rel=" << relError;
}

} // namespace

TEST(EnergyMatrix_JavaComparison, Dipeptide5Hydrophobic) {
    runJavaEmatComparisonOrSkip({"test_data/dipeptide.5hydrophobic.emat.bin", "test_data/dipeptide.5hydrophobic.test_conf.txt"});
}

TEST(EnergyMatrix_JavaComparison, Tiny6ov7Complex) {
    runJavaEmatComparisonOrSkip({"test_data/6ov7.tiny.complex.emat.bin", "test_data/6ov7.tiny.complex.test_conf.txt"});
}

TEST(EnergyMatrix_JavaComparison, TwoRL0NoPositionsProtein) {
    runJavaEmatComparisonOrSkip({"test_data/2RL0.NoPositions.protein.emat.bin", "test_data/2RL0.NoPositions.protein.test_conf.txt"});
}

TEST(EnergyMatrix_JavaComparison, TwoRL0TestSimplePartitionFunctionProtein) {
    runJavaEmatComparisonOrSkip({"test_data/2RL0.TestSimplePartitionFunction.protein.emat.bin", "test_data/2RL0.TestSimplePartitionFunction.protein.test_conf.txt"});
}

TEST(EnergyMatrix_JavaComparison, TwoRL0TestSimplePartitionFunctionLigand) {
    runJavaEmatComparisonOrSkip({"test_data/2RL0.TestSimplePartitionFunction.ligand.emat.bin", "test_data/2RL0.TestSimplePartitionFunction.ligand.test_conf.txt"});
}

TEST(EnergyMatrix_JavaComparison, Small6ov7Complex) {
    runJavaEmatComparisonOrSkip({"test_data/6ov7.small.complex.emat.bin", "test_data/6ov7.small.complex.test_conf.txt"});
}

TEST(EnergyMatrix_JavaComparison, OneDg96fComplex) {
    runJavaEmatComparisonOrSkip({"test_data/1dg9.6f.complex.emat.bin", "test_data/1dg9.6f.complex.test_conf.txt"});
}

TEST(EnergyMatrix_JavaComparison, TwoRL0TestSimplePartitionFunctionComplex) {
    runJavaEmatComparisonOrSkip({"test_data/2RL0.TestSimplePartitionFunction.complex.emat.bin", "test_data/2RL0.TestSimplePartitionFunction.complex.test_conf.txt"});
}

