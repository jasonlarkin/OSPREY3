#include <gtest/gtest.h>
#include "deepcopy.h"

TEST(DeepCopy, BasicInterface) {
    // Test that interface compiles and links
    uint8_t dummy[4] = {0, 0, 0, 0};
    
    void* result = deepCopyFromBuffer(dummy, 4);
    
    // For now, expect nullptr (not implemented yet)
    EXPECT_EQ(result, nullptr);
    
    const char* error = getLastError();
    EXPECT_NE(error, nullptr);
    EXPECT_STRNE(error, "");
    
    // Test free with nullptr (should be safe)
    freeDeepCopy(nullptr);
}

TEST(DeepCopy, NullInput) {
    // Test null pointer handling
    void* result = deepCopyFromBuffer(nullptr, 10);
    EXPECT_EQ(result, nullptr);
    
    const char* error = getLastError();
    EXPECT_NE(error, nullptr);
}

TEST(DeepCopy, ZeroSize) {
    // Test zero size handling
    uint8_t dummy[4] = {0, 0, 0, 0};
    void* result = deepCopyFromBuffer(dummy, 0);
    EXPECT_EQ(result, nullptr);
    
    const char* error = getLastError();
    EXPECT_NE(error, nullptr);
}

