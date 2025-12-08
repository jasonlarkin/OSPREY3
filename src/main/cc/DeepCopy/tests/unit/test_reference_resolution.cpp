#include <gtest/gtest.h>
#include "deserializer.h"
#include <vector>
#include <cstring>

/**
 * Unit tests for reference resolution.
 * 
 * Tests the resolveReferences() functionality:
 * - Valid reference graphs
 * - Circular references
 * - Invalid references (should be detected)
 * - Self-references
 */

class ReferenceResolutionTest : public ::testing::Test {
protected:
    // Helper to create a minimal valid stream header
    std::vector<uint8_t> createStreamHeader() {
        return {0xAC, 0xED, 0x00, 0x05};
    }
};

// Test that valid references are resolved correctly
TEST_F(ReferenceResolutionTest, ResolveValidReferences) {
    IterativeDeserializer deserializer;
    std::vector<uint8_t> stream = createStreamHeader();
    
    // Create two objects where second references first
    // First object: String "shared"
    stream.insert(stream.end(), {
        (uint8_t)StreamTag::TC_STRING, 0x00, 0x06, 's', 'h', 'a', 'r', 'e', 'd'
    });
    
    // Second object: ObjectWithString with reference to first String
    stream.insert(stream.end(), {
        (uint8_t)StreamTag::TC_OBJECT,
        (uint8_t)StreamTag::TC_CLASSDESC,
        (uint8_t)StreamTag::TC_STRING, 0x00, 0x10, 'O', 'b', 'j', 'e', 'c', 't', 'W', 'i', 't', 'h', 'S', 't', 'r', 'i', 'n', 'g',
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, // SerialVersionUID
        0x00, // Flags
        0x00, 0x01, // Field count (1)
        'L',
        (uint8_t)StreamTag::TC_STRING, 0x00, 0x12, 'L', 'j', 'a', 'v', 'a', '/', 'l', 'a', 'n', 'g', '/', 'S', 't', 'r', 'i', 'n', 'g', ';',
        (uint8_t)StreamTag::TC_STRING, 0x00, 0x04, 'n', 'a', 'm', 'e',
        (uint8_t)StreamTag::TC_ENDBLOCKDATA,
        (uint8_t)StreamTag::TC_NULL,
        (uint8_t)StreamTag::TC_REFERENCE, 0x00, 0x7E, 0x00, 0x01 // Reference to first String
    });

    auto result = deserializer.deserialize(stream.data(), stream.size());
    const char* error = deserializer.getLastError();
    if (error && strlen(error) > 0) {
        FAIL() << "Deserialization error: " << error;
    }
    ASSERT_NE(result, nullptr) << "Deserialization returned nullptr";
}

// Test circular reference resolution
TEST_F(ReferenceResolutionTest, ResolveCircularReferences) {
    IterativeDeserializer deserializer;
    std::vector<uint8_t> stream = createStreamHeader();
    
    // Create circular reference: A -> B -> A
    // Object A with reference to B
    stream.insert(stream.end(), {
        (uint8_t)StreamTag::TC_OBJECT,
        (uint8_t)StreamTag::TC_CLASSDESC,
        (uint8_t)StreamTag::TC_STRING, 0x00, 0x05, 'N', 'o', 'd', 'e', 'A',
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, // SerialVersionUID
        0x00, // Flags
        0x00, 0x01, // Field count (1)
        'L',
        (uint8_t)StreamTag::TC_STRING, 0x00, 0x07, 'L', 'N', 'o', 'd', 'e', 'B', ';',
        (uint8_t)StreamTag::TC_STRING, 0x00, 0x04, 'n', 'e', 'x', 't',
        (uint8_t)StreamTag::TC_ENDBLOCKDATA,
        (uint8_t)StreamTag::TC_NULL,
        // Will reference NodeB (handle 0x7E0002)
        (uint8_t)StreamTag::TC_REFERENCE, 0x00, 0x7E, 0x00, 0x02
    });
    
    // Object B with reference back to A
    stream.insert(stream.end(), {
        (uint8_t)StreamTag::TC_OBJECT,
        (uint8_t)StreamTag::TC_CLASSDESC,
        (uint8_t)StreamTag::TC_STRING, 0x00, 0x05, 'N', 'o', 'd', 'e', 'B',
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, // SerialVersionUID
        0x00, // Flags
        0x00, 0x01, // Field count (1)
        'L',
        (uint8_t)StreamTag::TC_STRING, 0x00, 0x07, 'L', 'N', 'o', 'd', 'e', 'A', ';',
        (uint8_t)StreamTag::TC_STRING, 0x00, 0x04, 'n', 'e', 'x', 't',
        (uint8_t)StreamTag::TC_ENDBLOCKDATA,
        (uint8_t)StreamTag::TC_NULL,
        // Reference back to NodeA (handle 0x7E0001)
        (uint8_t)StreamTag::TC_REFERENCE, 0x00, 0x7E, 0x00, 0x01
    });

    auto result = deserializer.deserialize(stream.data(), stream.size());
    const char* error = deserializer.getLastError();
    if (error && strlen(error) > 0) {
        FAIL() << "Deserialization error: " << error;
    }
    ASSERT_NE(result, nullptr) << "Circular references should be handled correctly";
}

// Test self-reference
TEST_F(ReferenceResolutionTest, ResolveSelfReference) {
    IterativeDeserializer deserializer;
    std::vector<uint8_t> stream = createStreamHeader();
    
    // Object that references itself
    stream.insert(stream.end(), {
        (uint8_t)StreamTag::TC_OBJECT,
        (uint8_t)StreamTag::TC_CLASSDESC,
        (uint8_t)StreamTag::TC_STRING, 0x00, 0x08, 'S', 'e', 'l', 'f', 'N', 'o', 'd', 'e',
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, // SerialVersionUID
        0x00, // Flags
        0x00, 0x01, // Field count (1)
        'L',
        (uint8_t)StreamTag::TC_STRING, 0x00, 0x0A, 'L', 'S', 'e', 'l', 'f', 'N', 'o', 'd', 'e', ';',
        (uint8_t)StreamTag::TC_STRING, 0x00, 0x04, 's', 'e', 'l', 'f',
        (uint8_t)StreamTag::TC_ENDBLOCKDATA,
        (uint8_t)StreamTag::TC_NULL,
        // Reference to itself (handle 0x7E0001)
        (uint8_t)StreamTag::TC_REFERENCE, 0x00, 0x7E, 0x00, 0x01
    });

    auto result = deserializer.deserialize(stream.data(), stream.size());
    const char* error = deserializer.getLastError();
    if (error && strlen(error) > 0) {
        FAIL() << "Deserialization error: " << error;
    }
    ASSERT_NE(result, nullptr) << "Self-references should be handled correctly";
}

// Test multiple references to same object
TEST_F(ReferenceResolutionTest, ResolveMultipleReferences) {
    IterativeDeserializer deserializer;
    std::vector<uint8_t> stream = createStreamHeader();
    
    // Shared object: String "shared"
    stream.insert(stream.end(), {
        (uint8_t)StreamTag::TC_STRING, 0x00, 0x06, 's', 'h', 'a', 'r', 'e', 'd'
    });
    
    // Object A references shared string
    stream.insert(stream.end(), {
        (uint8_t)StreamTag::TC_OBJECT,
        (uint8_t)StreamTag::TC_CLASSDESC,
        (uint8_t)StreamTag::TC_STRING, 0x00, 0x05, 'N', 'o', 'd', 'e', 'A',
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
        0x00, 0x00, 0x01,
        'L',
        (uint8_t)StreamTag::TC_STRING, 0x00, 0x12, 'L', 'j', 'a', 'v', 'a', '/', 'l', 'a', 'n', 'g', '/', 'S', 't', 'r', 'i', 'n', 'g', ';',
        (uint8_t)StreamTag::TC_STRING, 0x00, 0x04, 'n', 'a', 'm', 'e',
        (uint8_t)StreamTag::TC_ENDBLOCKDATA,
        (uint8_t)StreamTag::TC_NULL,
        (uint8_t)StreamTag::TC_REFERENCE, 0x00, 0x7E, 0x00, 0x01
    });
    
    // Object B also references same shared string
    stream.insert(stream.end(), {
        (uint8_t)StreamTag::TC_OBJECT,
        (uint8_t)StreamTag::TC_CLASSDESC,
        (uint8_t)StreamTag::TC_STRING, 0x00, 0x05, 'N', 'o', 'd', 'e', 'B',
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02,
        0x00, 0x00, 0x01,
        'L',
        (uint8_t)StreamTag::TC_STRING, 0x00, 0x12, 'L', 'j', 'a', 'v', 'a', '/', 'l', 'a', 'n', 'g', '/', 'S', 't', 'r', 'i', 'n', 'g', ';',
        (uint8_t)StreamTag::TC_STRING, 0x00, 0x04, 'n', 'a', 'm', 'e',
        (uint8_t)StreamTag::TC_ENDBLOCKDATA,
        (uint8_t)StreamTag::TC_NULL,
        (uint8_t)StreamTag::TC_REFERENCE, 0x00, 0x7E, 0x00, 0x01 // Same reference
    });

    auto result = deserializer.deserialize(stream.data(), stream.size());
    const char* error = deserializer.getLastError();
    if (error && strlen(error) > 0) {
        FAIL() << "Deserialization error: " << error;
    }
    ASSERT_NE(result, nullptr) << "Multiple references to same object should be handled";
}

