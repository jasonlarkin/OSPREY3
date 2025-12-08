#include <gtest/gtest.h>
#include "deserializer.h"
#include <vector>
#include <cstring>

/**
 * Unit tests for array parsing in Java serialization streams.
 * 
 * Tests parsing of TC_ARRAY structures:
 * - Primitive arrays (int[], double[], boolean[], etc.)
 * - Object arrays (String[], etc.)
 * - Multi-dimensional arrays
 * - Empty arrays
 * - Arrays with null elements
 * - Arrays with references to existing objects
 */

class ArrayParsingTest : public ::testing::Test {
protected:
    // Helper to create a minimal valid stream header
    std::vector<uint8_t> createStreamHeader() {
        return {0xAC, 0xED, 0x00, 0x05};
    }
};

// Test parsing int[] array
TEST_F(ArrayParsingTest, ParseIntArray) {
    IterativeDeserializer deserializer;
    std::vector<uint8_t> stream = createStreamHeader();
    
    // TC_ARRAY (0x75)
    // TC_CLASSDESC for "[I" (int array)
    // Array length: 3
    // Elements: 1, 2, 3
    stream.insert(stream.end(), {
        (uint8_t)StreamTag::TC_ARRAY,
        (uint8_t)StreamTag::TC_CLASSDESC,
        (uint8_t)StreamTag::TC_STRING, 0x00, 0x02, '[', 'I', // "[I"
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, // SerialVersionUID
        0x00, // Flags
        0x00, 0x00, // Field count (0)
        (uint8_t)StreamTag::TC_ENDBLOCKDATA,
        (uint8_t)StreamTag::TC_NULL,
        0x00, 0x00, 0x00, 0x03, // Array length: 3
        0x00, 0x00, 0x00, 0x01, // Element 0: 1
        0x00, 0x00, 0x00, 0x02, // Element 1: 2
        0x00, 0x00, 0x00, 0x03  // Element 2: 3
    });

    auto result = deserializer.deserialize(stream.data(), stream.size());
    const char* error = deserializer.getLastError();
    if (error && strlen(error) > 0) {
        FAIL() << "Deserialization error: " << error;
    }
    ASSERT_NE(result, nullptr) << "Deserialization returned nullptr. Error: " << (error ? error : "none");
}

// Test parsing double[] array
TEST_F(ArrayParsingTest, ParseDoubleArray) {
    IterativeDeserializer deserializer;
    std::vector<uint8_t> stream = createStreamHeader();
    
    // TC_ARRAY
    // TC_CLASSDESC for "[D" (double array)
    // Array length: 2
    // Elements: 1.5, 2.5
    stream.insert(stream.end(), {
        (uint8_t)StreamTag::TC_ARRAY,
        (uint8_t)StreamTag::TC_CLASSDESC,
        (uint8_t)StreamTag::TC_STRING, 0x00, 0x02, '[', 'D', // "[D"
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, // SerialVersionUID
        0x00, // Flags
        0x00, 0x00, // Field count (0)
        (uint8_t)StreamTag::TC_ENDBLOCKDATA,
        (uint8_t)StreamTag::TC_NULL,
        0x00, 0x00, 0x00, 0x02, // Array length: 2
        0x3F, 0xF8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Element 0: 1.5 (IEEE 754)
        0x40, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00  // Element 1: 2.5 (IEEE 754)
    });

    auto result = deserializer.deserialize(stream.data(), stream.size());
    const char* error = deserializer.getLastError();
    if (error && strlen(error) > 0) {
        FAIL() << "Deserialization error: " << error;
    }
    ASSERT_NE(result, nullptr) << "Deserialization returned nullptr. Error: " << (error ? error : "none");
}

// Test parsing boolean[] array
TEST_F(ArrayParsingTest, ParseBooleanArray) {
    IterativeDeserializer deserializer;
    std::vector<uint8_t> stream = createStreamHeader();
    
    // TC_ARRAY
    // TC_CLASSDESC for "[Z" (boolean array)
    // Array length: 3
    // Elements: true, false, true
    stream.insert(stream.end(), {
        (uint8_t)StreamTag::TC_ARRAY,
        (uint8_t)StreamTag::TC_CLASSDESC,
        (uint8_t)StreamTag::TC_STRING, 0x00, 0x02, '[', 'Z', // "[Z"
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, // SerialVersionUID
        0x00, // Flags
        0x00, 0x00, // Field count (0)
        (uint8_t)StreamTag::TC_ENDBLOCKDATA,
        (uint8_t)StreamTag::TC_NULL,
        0x00, 0x00, 0x00, 0x03, // Array length: 3
        0x01, // Element 0: true
        0x00, // Element 1: false
        0x01  // Element 2: true
    });

    auto result = deserializer.deserialize(stream.data(), stream.size());
    const char* error = deserializer.getLastError();
    if (error && strlen(error) > 0) {
        FAIL() << "Deserialization error: " << error;
    }
    ASSERT_NE(result, nullptr) << "Deserialization returned nullptr. Error: " << (error ? error : "none");
}

// Test parsing empty array
TEST_F(ArrayParsingTest, ParseEmptyArray) {
    IterativeDeserializer deserializer;
    std::vector<uint8_t> stream = createStreamHeader();
    
    // TC_ARRAY
    // TC_CLASSDESC for "[I" (int array)
    // Array length: 0
    stream.insert(stream.end(), {
        (uint8_t)StreamTag::TC_ARRAY,
        (uint8_t)StreamTag::TC_CLASSDESC,
        (uint8_t)StreamTag::TC_STRING, 0x00, 0x02, '[', 'I', // "[I"
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, // SerialVersionUID
        0x00, // Flags
        0x00, 0x00, // Field count (0)
        (uint8_t)StreamTag::TC_ENDBLOCKDATA,
        (uint8_t)StreamTag::TC_NULL,
        0x00, 0x00, 0x00, 0x00  // Array length: 0
    });

    auto result = deserializer.deserialize(stream.data(), stream.size());
    const char* error = deserializer.getLastError();
    if (error && strlen(error) > 0) {
        FAIL() << "Deserialization error: " << error;
    }
    ASSERT_NE(result, nullptr) << "Deserialization returned nullptr. Error: " << (error ? error : "none");
}

// Test parsing String[] array
TEST_F(ArrayParsingTest, ParseStringArray) {
    IterativeDeserializer deserializer;
    std::vector<uint8_t> stream = createStreamHeader();
    
    // TC_ARRAY
    // TC_CLASSDESC for "[Ljava/lang/String;" (String array)
    // Array length: 2
    // Element 0: TC_STRING "hello"
    // Element 1: TC_STRING "world"
    stream.insert(stream.end(), {
        (uint8_t)StreamTag::TC_ARRAY,
        (uint8_t)StreamTag::TC_CLASSDESC,
        (uint8_t)StreamTag::TC_STRING, 0x00, 0x13, '[', 'L', 'j', 'a', 'v', 'a', '/', 'l', 'a', 'n', 'g', '/', 'S', 't', 'r', 'i', 'n', 'g', ';', // "[Ljava/lang/String;" (19 chars)
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05, // SerialVersionUID
        0x00, // Flags
        0x00, 0x00, // Field count (0)
        (uint8_t)StreamTag::TC_ENDBLOCKDATA,
        (uint8_t)StreamTag::TC_NULL,
        0x00, 0x00, 0x00, 0x02, // Array length: 2
        (uint8_t)StreamTag::TC_STRING, 0x00, 0x05, 'h', 'e', 'l', 'l', 'o', // Element 0
        (uint8_t)StreamTag::TC_STRING, 0x00, 0x05, 'w', 'o', 'r', 'l', 'd'  // Element 1
    });

    auto result = deserializer.deserialize(stream.data(), stream.size());
    const char* error = deserializer.getLastError();
    if (error && strlen(error) > 0) {
        FAIL() << "Deserialization error: " << error;
    }
    ASSERT_NE(result, nullptr) << "Deserialization returned nullptr. Error: " << (error ? error : "none");
}

// Test parsing String[] array with null elements
TEST_F(ArrayParsingTest, ParseStringArrayWithNulls) {
    IterativeDeserializer deserializer;
    std::vector<uint8_t> stream = createStreamHeader();
    
    // TC_ARRAY
    // TC_CLASSDESC for "[Ljava/lang/String;"
    // Array length: 3
    // Element 0: TC_STRING "hello"
    // Element 1: TC_NULL
    // Element 2: TC_STRING "world"
    stream.insert(stream.end(), {
        (uint8_t)StreamTag::TC_ARRAY,
        (uint8_t)StreamTag::TC_CLASSDESC,
        (uint8_t)StreamTag::TC_STRING, 0x00, 0x13, '[', 'L', 'j', 'a', 'v', 'a', '/', 'l', 'a', 'n', 'g', '/', 'S', 't', 'r', 'i', 'n', 'g', ';', // "[Ljava/lang/String;" (19 chars)
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, // SerialVersionUID
        0x00, // Flags
        0x00, 0x00, // Field count (0)
        (uint8_t)StreamTag::TC_ENDBLOCKDATA,
        (uint8_t)StreamTag::TC_NULL,
        0x00, 0x00, 0x00, 0x03, // Array length: 3
        (uint8_t)StreamTag::TC_STRING, 0x00, 0x05, 'h', 'e', 'l', 'l', 'o', // Element 0
        (uint8_t)StreamTag::TC_NULL, // Element 1: null
        (uint8_t)StreamTag::TC_STRING, 0x00, 0x05, 'w', 'o', 'r', 'l', 'd'  // Element 2
    });

    auto result = deserializer.deserialize(stream.data(), stream.size());
    const char* error = deserializer.getLastError();
    if (error && strlen(error) > 0) {
        FAIL() << "Deserialization error: " << error;
    }
    ASSERT_NE(result, nullptr) << "Deserialization returned nullptr. Error: " << (error ? error : "none");
}

// Test parsing String[] array with references
TEST_F(ArrayParsingTest, ParseStringArrayWithReferences) {
    IterativeDeserializer deserializer;
    std::vector<uint8_t> stream = createStreamHeader();
    
    // First String: "shared"
    stream.insert(stream.end(), {
        (uint8_t)StreamTag::TC_STRING, 0x00, 0x06, 's', 'h', 'a', 'r', 'e', 'd'
    });
    
    // TC_ARRAY
    // TC_CLASSDESC for "[Ljava/lang/String;"
    // Array length: 2
    // Element 0: TC_STRING "new"
    // Element 1: TC_REFERENCE to "shared"
    stream.insert(stream.end(), {
        (uint8_t)StreamTag::TC_ARRAY,
        (uint8_t)StreamTag::TC_CLASSDESC,
        (uint8_t)StreamTag::TC_STRING, 0x00, 0x14, '[', 'L', 'j', 'a', 'v', 'a', '/', 'l', 'a', 'n', 'g', '/', 'S', 't', 'r', 'i', 'n', 'g', ';',
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, // SerialVersionUID
        0x00, // Flags
        0x00, 0x00, // Field count (0)
        (uint8_t)StreamTag::TC_ENDBLOCKDATA,
        (uint8_t)StreamTag::TC_NULL,
        0x00, 0x00, 0x00, 0x02, // Array length: 2
        (uint8_t)StreamTag::TC_STRING, 0x00, 0x03, 'n', 'e', 'w', // Element 0
        (uint8_t)StreamTag::TC_REFERENCE, 0x00, 0x7E, 0x00, 0x01 // Element 1: reference to "shared" (handle 0x7E0001)
    });

    auto result = deserializer.deserialize(stream.data(), stream.size());
    const char* error = deserializer.getLastError();
    if (error && strlen(error) > 0) {
        FAIL() << "Deserialization error: " << error;
    }
    ASSERT_NE(result, nullptr) << "Deserialization returned nullptr. Error: " << (error ? error : "none");
}

// Test parsing multi-dimensional array (int[][])
TEST_F(ArrayParsingTest, ParseMultiDimensionalArray) {
    IterativeDeserializer deserializer;
    std::vector<uint8_t> stream = createStreamHeader();
    
    // TC_ARRAY
    // TC_CLASSDESC for "[[I" (int[][] array)
    // Array length: 2
    // Element 0: TC_ARRAY (nested int[] array)
    // Element 1: TC_ARRAY (nested int[] array)
    stream.insert(stream.end(), {
        (uint8_t)StreamTag::TC_ARRAY,
        (uint8_t)StreamTag::TC_CLASSDESC,
        (uint8_t)StreamTag::TC_STRING, 0x00, 0x03, '[', '[', 'I', // "[[I"
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, // SerialVersionUID
        0x00, // Flags
        0x00, 0x00, // Field count (0)
        (uint8_t)StreamTag::TC_ENDBLOCKDATA,
        (uint8_t)StreamTag::TC_NULL,
        0x00, 0x00, 0x00, 0x02, // Array length: 2
        // Element 0: nested int[] array
        (uint8_t)StreamTag::TC_ARRAY,
        (uint8_t)StreamTag::TC_CLASSDESC,
        (uint8_t)StreamTag::TC_STRING, 0x00, 0x02, '[', 'I', // "[I"
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x09, // SerialVersionUID
        0x00, // Flags
        0x00, 0x00, // Field count (0)
        (uint8_t)StreamTag::TC_ENDBLOCKDATA,
        (uint8_t)StreamTag::TC_NULL,
        0x00, 0x00, 0x00, 0x02, // Nested array length: 2
        0x00, 0x00, 0x00, 0x01, // Nested element 0: 1
        0x00, 0x00, 0x00, 0x02, // Nested element 1: 2
        // Element 1: nested int[] array
        (uint8_t)StreamTag::TC_ARRAY,
        (uint8_t)StreamTag::TC_REFERENCE, 0x00, 0x7E, 0x00, 0x03, // Reference to "[I" class descriptor (handle 0x7E0003: class name at 0x7E0002, descriptor at 0x7E0003)
        0x00, 0x00, 0x00, 0x02, // Nested array length: 2
        0x00, 0x00, 0x00, 0x03, // Nested element 0: 3
        0x00, 0x00, 0x00, 0x04  // Nested element 1: 4
    });

    auto result = deserializer.deserialize(stream.data(), stream.size());
    const char* error = deserializer.getLastError();
    if (error && strlen(error) > 0) {
        FAIL() << "Deserialization error: " << error;
    }
    ASSERT_NE(result, nullptr) << "Deserialization returned nullptr. Error: " << (error ? error : "none");
}

// Test parsing array with class descriptor reference
TEST_F(ArrayParsingTest, ParseArrayWithClassDescriptorReference) {
    IterativeDeserializer deserializer;
    std::vector<uint8_t> stream = createStreamHeader();
    
    // First array: int[] with class descriptor
    stream.insert(stream.end(), {
        (uint8_t)StreamTag::TC_ARRAY,
        (uint8_t)StreamTag::TC_CLASSDESC,
        (uint8_t)StreamTag::TC_STRING, 0x00, 0x02, '[', 'I', // "[I"
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0A, // SerialVersionUID
        0x00, // Flags
        0x00, 0x00, // Field count (0)
        (uint8_t)StreamTag::TC_ENDBLOCKDATA,
        (uint8_t)StreamTag::TC_NULL,
        0x00, 0x00, 0x00, 0x01, // Array length: 1
        0x00, 0x00, 0x00, 0x2A  // Element: 42
    });
    
    // Second array: int[] with reference to class descriptor
    stream.insert(stream.end(), {
        (uint8_t)StreamTag::TC_ARRAY,
        (uint8_t)StreamTag::TC_REFERENCE,
        0x7E, 0x00, 0x00, 0x00, // Reference to "[I" class descriptor (handle 0x7E0000)
        0x00, 0x00, 0x00, 0x01, // Array length: 1
        0x00, 0x00, 0x00, 0x64  // Element: 100
    });

    auto result = deserializer.deserialize(stream.data(), stream.size());
    const char* error = deserializer.getLastError();
    if (error && strlen(error) > 0) {
        FAIL() << "Deserialization error: " << error;
    }
    ASSERT_NE(result, nullptr) << "Deserialization returned nullptr. Error: " << (error ? error : "none");
}

