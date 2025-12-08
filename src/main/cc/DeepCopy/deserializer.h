#ifndef DESERIALIZER_H
#define DESERIALIZER_H

#include <cstdint>
#include <cstddef>
#include <memory>
#include <vector>
#include <queue>
#include <unordered_map>
#include <string>

/**
 * Iterative deserializer for Java object serialization streams.
 * 
 * Uses BFS traversal with work queue to avoid recursion limits.
 */

// Java serialization stream tags
enum class StreamTag : uint8_t {
    TC_NULL = 0x70,
    TC_REFERENCE = 0x71,
    TC_CLASSDESC = 0x72,
    TC_OBJECT = 0x73,
    TC_STRING = 0x74,
    TC_ARRAY = 0x75,
    TC_CLASS = 0x76,
    TC_BLOCKDATA = 0x77,
    TC_ENDBLOCKDATA = 0x78,
    TC_EXCEPTION = 0x7B,
    TC_LONGSTRING = 0x7C,
    TC_PROXYCLASSDESC = 0x7D,
    TC_ENUM = 0x7E
};

// Work queue item
struct DeserializationTask {
    size_t nodeIndex;
    size_t streamOffset;  // Offset in stream instead of pointer
};

// Field descriptor in class descriptor
struct FieldDescriptor {
    char typeCode;  // 'I'=int, 'J'=long, 'D'=double, 'Z'=boolean, 'L'=object, etc.
    std::string typeName;  // For object types: "Ljava/lang/String;"
    std::string fieldName;
};

// Class descriptor information
struct ClassDescriptor {
    std::string className;
    uint64_t serialVersionUID;
    uint8_t flags;
    std::vector<FieldDescriptor> fields;
    size_t handleId;
    
    ClassDescriptor() : serialVersionUID(0), flags(0), handleId(0) {}
};

class IterativeDeserializer {
public:
    // Custom deleter for object graph
    struct ObjectGraphDeleter {
        void operator()(void* ptr) const {
            if (ptr) {
                // TODO: Implement proper cleanup
                delete static_cast<uint8_t*>(ptr);
            }
        }
    };
    
    using ObjectGraphPtr = std::unique_ptr<void, ObjectGraphDeleter>;
    
    // Deserialization node structure
    struct DeserializationNode {
        ObjectGraphPtr object;
        std::vector<size_t> references;  // Indices of referenced nodes
        std::vector<size_t> referenceOffsets;  // Stream offsets where references occur
        size_t classId;
        size_t handleId;  // Handle ID in Java serialization stream
        bool visited;
        
        DeserializationNode();
    };
    
    /**
     * Deserialize Java object from serialized buffer.
     * 
     * @param serializedData Pointer to serialized data
     * @param dataSize Size of data in bytes
     * @return Unique pointer to deserialized object, or nullptr on error
     */
    ObjectGraphPtr deserialize(
        const void* serializedData, 
        size_t dataSize);
    
    /**
     * Get last error message.
     */
    const char* getLastError() const { return lastError.c_str(); }

private:
    std::queue<DeserializationTask> workQueue;
    std::vector<std::unique_ptr<DeserializationNode>> nodes;
    std::unordered_map<size_t, size_t> handleToNode;  // Handle ID -> node index
    std::unordered_map<size_t, size_t> offsetToNode;  // Stream offset -> node index
    std::unordered_map<size_t, ClassDescriptor> classDescriptors;  // Handle ID -> class descriptor
    std::unordered_map<size_t, std::string> handleToString;  // Handle ID -> string value
    const uint8_t* stream;
    size_t streamSize;
    size_t streamPos;
    size_t nextHandleId;  // Next handle ID to assign
    std::string lastError;
    
    // Stream parsing utilities
    void parseStreamHeader();
    StreamTag readTag(size_t offset);
    uint8_t readByte(size_t offset);
    uint16_t readShort(size_t offset);
    uint32_t readInt(size_t offset);
    uint64_t readLong(size_t offset);
    std::string readString(size_t offset);
    size_t readHandle(size_t offset);
    void skipBlockData(size_t& offset);
    size_t skipObjectData(size_t offset);  // Skip over object/array/string, return new offset
    
    // Node management
    size_t createNode();
    size_t getOrCreateNodeForHandle(size_t handleId);
    
    // Deserialization
    void processNode(const DeserializationTask& task);
    void processObject(size_t nodeIndex, size_t offset);
    size_t processClassDesc(size_t offset, size_t& handleId); // Returns offset after class descriptor, handleId is set
    void processArray(size_t nodeIndex, size_t offset);
    void processString(size_t nodeIndex, size_t offset);
    
    // Reference handling
    void processReferences(size_t nodeIndex);
    void resolveReferences();
};

#endif

