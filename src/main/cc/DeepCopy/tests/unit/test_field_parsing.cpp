#include <gtest/gtest.h>
#include "deserializer.h"
#include <vector>
#include <cstring>

/**
 * Unit tests for object field parsing.
 * 
 * Tests parsing of object fields from Java serialization streams:
 * - Primitive fields (int, double, boolean, etc.)
 * - Object reference fields
 * - Array fields
 * - Nested object structures
 */

class FieldParsingTest : public ::testing::Test {
protected:
    // Helper to create a minimal valid stream header
    std::vector<uint8_t> createStreamHeader() {
        return {0xAC, 0xED, 0x00, 0x05};
    }
};

// Test parsing object with int primitive field
TEST_F(FieldParsingTest, ParseObjectWithIntField) {
    IterativeDeserializer deserializer;
    std::vector<uint8_t> stream = createStreamHeader();
    
    // TC_OBJECT (0x73)
    // TC_CLASSDESC (0x72)
    // TC_STRING "IntObject" (length 9)
    // SerialVersionUID (long) 0x0000000000000001
    // Flags (byte) 0x00
    // Field count (short) 0x0001
    // Field: 'I' (int), TC_STRING "value" (length 5)
    // TC_ENDBLOCKDATA (0x78)
    // TC_NULL (0x70) - no superclass
    // Field value: int 42 (0x0000002A)
    stream.insert(stream.end(), {
        (uint8_t)StreamTag::TC_OBJECT,
        (uint8_t)StreamTag::TC_CLASSDESC,
        (uint8_t)StreamTag::TC_STRING, 0x00, 0x09, 'I', 'n', 't', 'O', 'b', 'j', 'e', 'c', 't',
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, // SerialVersionUID
        0x00, // Flags
        0x00, 0x01, // Field count (1)
        'I', // Type: int
        (uint8_t)StreamTag::TC_STRING, 0x00, 0x05, 'v', 'a', 'l', 'u', 'e', // Field name
        (uint8_t)StreamTag::TC_ENDBLOCKDATA,
        (uint8_t)StreamTag::TC_NULL,
        0x00, 0x00, 0x00, 0x2A // Field value: 42
    });

    auto result = deserializer.deserialize(stream.data(), stream.size());
    const char* error = deserializer.getLastError();
    if (error && strlen(error) > 0) {
        FAIL() << "Deserialization error: " << error;
    }
    ASSERT_NE(result, nullptr) << "Deserialization returned nullptr. Error: " << (error ? error : "none");
}

// Test parsing object with multiple primitive fields
TEST_F(FieldParsingTest, ParseObjectWithMultiplePrimitives) {
    IterativeDeserializer deserializer;
    std::vector<uint8_t> stream = createStreamHeader();
    
    // Object with: int, double, boolean fields
    stream.insert(stream.end(), {
        (uint8_t)StreamTag::TC_OBJECT,
        (uint8_t)StreamTag::TC_CLASSDESC,
        (uint8_t)StreamTag::TC_STRING, 0x00, 0x0E, 'M', 'u', 'l', 't', 'i', 'P', 'r', 'i', 'm', 'i', 't', 'i', 'v', 'e',
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, // SerialVersionUID
        0x00, // Flags
        0x00, 0x03, // Field count (3)
        'I', (uint8_t)StreamTag::TC_STRING, 0x00, 0x05, 'v', 'a', 'l', 'u', 'e', // int value
        'D', (uint8_t)StreamTag::TC_STRING, 0x00, 0x04, 'd', 'a', 't', 'a', // double data
        'Z', (uint8_t)StreamTag::TC_STRING, 0x00, 0x04, 'f', 'l', 'a', 'g', // boolean flag
        (uint8_t)StreamTag::TC_ENDBLOCKDATA,
        (uint8_t)StreamTag::TC_NULL,
        // Field values: int 100, double 3.14, boolean true
        0x00, 0x00, 0x00, 0x64, // int: 100
        0x40, 0x09, 0x1E, 0xB8, 0x51, 0xEB, 0x85, 0x1F, // double: 3.14 (IEEE 754)
        0x01 // boolean: true
    });

    auto result = deserializer.deserialize(stream.data(), stream.size());
    const char* error = deserializer.getLastError();
    if (error && strlen(error) > 0) {
        FAIL() << "Deserialization error: " << error;
    }
    ASSERT_NE(result, nullptr) << "Deserialization returned nullptr. Error: " << (error ? error : "none");
}

// Test parsing object with object reference field
TEST_F(FieldParsingTest, ParseObjectWithObjectField) {
    IterativeDeserializer deserializer;
    std::vector<uint8_t> stream = createStreamHeader();
    
    // Object with String field
    // TC_OBJECT
    // TC_CLASSDESC "ObjectWithString"
    // Field: 'L' (object), type "Ljava/lang/String;", name "name"
    // TC_ENDBLOCKDATA, TC_NULL
    // Field value: TC_STRING "test"
    stream.insert(stream.end(), {
        (uint8_t)StreamTag::TC_OBJECT,
        (uint8_t)StreamTag::TC_CLASSDESC,
        (uint8_t)StreamTag::TC_STRING, 0x00, 0x10, 'O', 'b', 'j', 'e', 'c', 't', 'W', 'i', 't', 'h', 'S', 't', 'r', 'i', 'n', 'g',
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, // SerialVersionUID
        0x00, // Flags
        0x00, 0x01, // Field count (1)
        'L', // Object type
        (uint8_t)StreamTag::TC_STRING, 0x00, 0x12, 'L', 'j', 'a', 'v', 'a', '/', 'l', 'a', 'n', 'g', '/', 'S', 't', 'r', 'i', 'n', 'g', ';', // Type name
        (uint8_t)StreamTag::TC_STRING, 0x00, 0x04, 'n', 'a', 'm', 'e', // Field name
        (uint8_t)StreamTag::TC_ENDBLOCKDATA,
        (uint8_t)StreamTag::TC_NULL,
        (uint8_t)StreamTag::TC_STRING, 0x00, 0x04, 't', 'e', 's', 't' // String value
    });

    auto result = deserializer.deserialize(stream.data(), stream.size());
    const char* error = deserializer.getLastError();
    if (error && strlen(error) > 0) {
        FAIL() << "Deserialization error: " << error;
    }
    ASSERT_NE(result, nullptr) << "Deserialization returned nullptr. Error: " << (error ? error : "none");
}

// Test parsing object with null object field
TEST_F(FieldParsingTest, ParseObjectWithNullField) {
    IterativeDeserializer deserializer;
    std::vector<uint8_t> stream = createStreamHeader();
    
    // Object with null String field
    stream.insert(stream.end(), {
        (uint8_t)StreamTag::TC_OBJECT,
        (uint8_t)StreamTag::TC_CLASSDESC,
        (uint8_t)StreamTag::TC_STRING, 0x00, 0x0E, 'O', 'b', 'j', 'e', 'c', 't', 'W', 'i', 't', 'h', 'N', 'u', 'l', 'l',
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, // SerialVersionUID
        0x00, // Flags
        0x00, 0x01, // Field count (1)
        'L', // Object type
        (uint8_t)StreamTag::TC_STRING, 0x00, 0x12, 'L', 'j', 'a', 'v', 'a', '/', 'l', 'a', 'n', 'g', '/', 'S', 't', 'r', 'i', 'n', 'g', ';',
        (uint8_t)StreamTag::TC_STRING, 0x00, 0x04, 'n', 'a', 'm', 'e',
        (uint8_t)StreamTag::TC_ENDBLOCKDATA,
        (uint8_t)StreamTag::TC_NULL,
        (uint8_t)StreamTag::TC_NULL // Null field value
    });

    auto result = deserializer.deserialize(stream.data(), stream.size());
    const char* error = deserializer.getLastError();
    if (error && strlen(error) > 0) {
        FAIL() << "Deserialization error: " << error;
    }
    ASSERT_NE(result, nullptr) << "Deserialization returned nullptr. Error: " << (error ? error : "none");
}

// Test parsing object with reference to existing object
TEST_F(FieldParsingTest, ParseObjectWithReferenceField) {
    IterativeDeserializer deserializer;
    std::vector<uint8_t> stream = createStreamHeader();
    
    // First object: String "shared"
    stream.insert(stream.end(), {
        (uint8_t)StreamTag::TC_STRING, 0x00, 0x06, 's', 'h', 'a', 'r', 'e', 'd'
    });
    
    // Second object: ObjectWithString with reference to first String
    stream.insert(stream.end(), {
        (uint8_t)StreamTag::TC_OBJECT,
        (uint8_t)StreamTag::TC_CLASSDESC,
        (uint8_t)StreamTag::TC_STRING, 0x00, 0x10, 'O', 'b', 'j', 'e', 'c', 't', 'W', 'i', 't', 'h', 'S', 't', 'r', 'i', 'n', 'g',
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05, // SerialVersionUID
        0x00, // Flags
        0x00, 0x01, // Field count (1)
        'L',
        (uint8_t)StreamTag::TC_STRING, 0x00, 0x12, 'L', 'j', 'a', 'v', 'a', '/', 'l', 'a', 'n', 'g', '/', 'S', 't', 'r', 'i', 'n', 'g', ';',
        (uint8_t)StreamTag::TC_STRING, 0x00, 0x04, 'n', 'a', 'm', 'e',
        (uint8_t)StreamTag::TC_ENDBLOCKDATA,
        (uint8_t)StreamTag::TC_NULL,
        (uint8_t)StreamTag::TC_REFERENCE, 0x00, 0x7E, 0x00, 0x01 // Reference to first String (handle 0x7E0001)
    });

    auto result = deserializer.deserialize(stream.data(), stream.size());
    const char* error = deserializer.getLastError();
    if (error && strlen(error) > 0) {
        FAIL() << "Deserialization error: " << error;
    }
    ASSERT_NE(result, nullptr) << "Deserialization returned nullptr. Error: " << (error ? error : "none");
}

// Test parsing nested objects
TEST_F(FieldParsingTest, ParseNestedObjects) {
    IterativeDeserializer deserializer;
    std::vector<uint8_t> stream = createStreamHeader();
    
    // Outer object contains inner object
    // Inner object: IntObject with value 42
    // Outer object: ContainerObject with IntObject field
    
    // First, define inner object class
    stream.insert(stream.end(), {
        (uint8_t)StreamTag::TC_OBJECT,
        (uint8_t)StreamTag::TC_CLASSDESC,
        (uint8_t)StreamTag::TC_STRING, 0x00, 0x09, 'I', 'n', 't', 'O', 'b', 'j', 'e', 'c', 't',
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, // SerialVersionUID
        0x00, // Flags
        0x00, 0x01, // Field count (1)
        'I',
        (uint8_t)StreamTag::TC_STRING, 0x00, 0x05, 'v', 'a', 'l', 'u', 'e',
        (uint8_t)StreamTag::TC_ENDBLOCKDATA,
        (uint8_t)StreamTag::TC_NULL,
        0x00, 0x00, 0x00, 0x2A // Value: 42
    });
    
    // Then, define outer object class and instance
    stream.insert(stream.end(), {
        (uint8_t)StreamTag::TC_OBJECT,
        (uint8_t)StreamTag::TC_CLASSDESC,
        (uint8_t)StreamTag::TC_STRING, 0x00, 0x0E, 'C', 'o', 'n', 't', 'a', 'i', 'n', 'e', 'r', 'O', 'b', 'j', 'e', 'c', 't',
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, // SerialVersionUID
        0x00, // Flags
        0x00, 0x01, // Field count (1)
        'L',
        (uint8_t)StreamTag::TC_STRING, 0x00, 0x0A, 'L', 'I', 'n', 't', 'O', 'b', 'j', 'e', 'c', 't', ';',
        (uint8_t)StreamTag::TC_STRING, 0x00, 0x05, 'c', 'h', 'i', 'l', 'd',
        (uint8_t)StreamTag::TC_ENDBLOCKDATA,
        (uint8_t)StreamTag::TC_NULL,
        // Reference to first IntObject
        (uint8_t)StreamTag::TC_REFERENCE, 0x00, 0x7E, 0x00, 0x01
    });

    auto result = deserializer.deserialize(stream.data(), stream.size());
    const char* error = deserializer.getLastError();
    if (error && strlen(error) > 0) {
        FAIL() << "Deserialization error: " << error;
    }
    ASSERT_NE(result, nullptr) << "Deserialization returned nullptr. Error: " << (error ? error : "none");
}

