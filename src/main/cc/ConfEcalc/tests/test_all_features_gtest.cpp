
#include <gtest/gtest.h>
#include <concepts>
#include <type_traits>

// Test template using concepts
template<std::floating_point T>
T test_energy_calc(T x, T y) {
    return x * y + 1.5;
}

// Test with [[nodiscard]]
[[nodiscard]] int test_get_energy() {
    return 42;
}

// Test with noexcept
template<std::floating_point T>
[[nodiscard]] T test_safe_get(T* ptr, int index) noexcept {
    return ptr[index];
}

// All features together
template<std::floating_point T>
class EnergyCalculator {
public:
    explicit EnergyCalculator(int size) noexcept : size_(size) {}
    
    [[nodiscard]] int get_size() const noexcept {
        return size_;
    }
    
    template<std::floating_point U>
    [[nodiscard]] U calculate_energy(const U* coords, int count) const noexcept {
        U total = 0.0;
        for (int i = 0; i < count; ++i) {
            total += coords[i] * coords[i];
        }
        return total;
    }
    
    template<std::floating_point U>
    [[nodiscard]] U distance_sq(const U* a, const U* b) const noexcept {
        U dx = a[0] - b[0];
        U dy = a[1] - b[1];
        U dz = a[2] - b[2];
        return dx*dx + dy*dy + dz*dz;
    }
    
private:
    int size_;
};

// Test Concepts: All Features Together
TEST(AllFeaturesTest, ConceptsAcceptFloat) {
    float f1 = 2.5f, f2 = 3.0f;
    float result = test_energy_calc(f1, f2);
    EXPECT_FLOAT_EQ(result, 9.0f);
}

TEST(AllFeaturesTest, ConceptsAcceptDouble) {
    double d1 = 2.5, d2 = 3.0;
    double result = test_energy_calc(d1, d2);
    EXPECT_DOUBLE_EQ(result, 9.0);
}

TEST(AllFeaturesTest, ConceptsRejectInt) {
    static_assert(!std::floating_point<int>);
    // int i1 = 2, i2 = 3;
    // test_energy_calc(i1, i2);  // Would fail to compile
}

TEST(AllFeaturesTest, NoDiscardWorks) {
    int energy = test_get_energy();
    EXPECT_EQ(energy, 42);
    // test_get_energy();  // Would generate warning if uncommented
}

TEST(AllFeaturesTest, NoexceptWorks) {
    float arr[3] = {1.0f, 2.0f, 3.0f};
    float val = test_safe_get(arr, 1);
    EXPECT_FLOAT_EQ(val, 2.0f);
    static_assert(noexcept(test_safe_get(arr, 1)));
}

TEST(AllFeaturesTest, AllFeaturesTogether) {
    EnergyCalculator<float> calc(10);
    
    EXPECT_EQ(calc.get_size(), 10);
    static_assert(noexcept(calc.get_size()));
    
    float coords[3] = {1.0f, 2.0f, 3.0f};
    float energy = calc.calculate_energy(coords, 3);
    EXPECT_FLOAT_EQ(energy, 14.0f);
    
    float a[3] = {0.0f, 0.0f, 0.0f};
    float b[3] = {1.0f, 1.0f, 1.0f};
    float dist_sq = calc.distance_sq(a, b);
    EXPECT_FLOAT_EQ(dist_sq, 3.0f);
}

TEST(AllFeaturesTest, RealWorldPattern) {
    EnergyCalculator<double> energy_calc(100);
    
    double atom1[3] = {1.0, 2.0, 3.0};
    double atom2[3] = {4.0, 5.0, 6.0};
    double atom3[3] = {7.0, 8.0, 9.0};
    
    double e1 = energy_calc.calculate_energy(atom1, 3);
    double e2 = energy_calc.calculate_energy(atom2, 3);
    double e3 = energy_calc.calculate_energy(atom3, 3);
    
    EXPECT_DOUBLE_EQ(e1, 14.0);
    EXPECT_DOUBLE_EQ(e2, 77.0);
    EXPECT_DOUBLE_EQ(e3, 194.0);
    
    double dist12 = energy_calc.distance_sq(atom1, atom2);
    double dist23 = energy_calc.distance_sq(atom2, atom3);
    
    EXPECT_DOUBLE_EQ(dist12, 27.0);
    EXPECT_DOUBLE_EQ(dist23, 27.0);
}

TEST(AllFeaturesTest, CompileTimeValidation) {
    static_assert(std::floating_point<float>);
    static_assert(std::floating_point<double>);
    static_assert(!std::floating_point<int>);
    
    float arr[3] = {1.0f, 2.0f, 3.0f};
    static_assert(noexcept(test_safe_get(arr, 1)));
    
    SUCCEED();
}

