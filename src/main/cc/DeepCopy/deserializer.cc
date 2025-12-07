#include "deserializer.h"
#include <stdexcept>
#include <cstring>

IterativeDeserializer::DeserializationNode::DeserializationNode() 
    : object(IterativeDeserializer::ObjectGraphPtr(nullptr, IterativeDeserializer::ObjectGraphDeleter())), 
      classId(0), 
      handleId(0),
      visited(false) {
}

IterativeDeserializer::ObjectGraphPtr IterativeDeserializer::deserialize(
    const void* serializedData, 
    size_t dataSize) {
    
    stream = static_cast<const uint8_t*>(serializedData);
    streamSize = dataSize;
    streamPos = 0;
    nextHandleId = 0x007E0000;  // Java serialization starts handles at 0x007E0000
    lastError.clear();
    
    // Clear state
    while (!workQueue.empty()) {
        workQueue.pop();
    }
    nodes.clear();
    handleToNode.clear();
    offsetToNode.clear();
    
    try {
        // Parse Java serialization stream header
        parseStreamHeader();
        
        // Read root object tag
        StreamTag rootTag = readTag(streamPos);
        if (rootTag == StreamTag::TC_NULL) {
            lastError = "Root object is null";
            return ObjectGraphPtr(nullptr, ObjectGraphDeleter());
        }
        
        // Create root node and start processing
        size_t rootNode = createNode();
        workQueue.push({rootNode, streamPos});
        
        // Iterative BFS traversal
        while (!workQueue.empty()) {
            DeserializationTask task = workQueue.front();
            workQueue.pop();
            
            // Process this node's object data
            processNode(task);
            
            // Handle all references from this node
            processReferences(task.nodeIndex);
        }
        
        // Resolve all object references
        resolveReferences();
        
        // Transfer ownership of root object
        if (nodes.empty() || !nodes[0]->object) {
            lastError = "Failed to deserialize root object";
            return ObjectGraphPtr(nullptr, ObjectGraphDeleter());
        }
        
        return std::move(nodes[0]->object);
        
    } catch (const std::exception& e) {
        lastError = std::string("Exception: ") + e.what();
        return ObjectGraphPtr(nullptr, ObjectGraphDeleter());
    } catch (...) {
        lastError = "Unknown exception";
        return ObjectGraphPtr(nullptr, ObjectGraphDeleter());
    }
}

void IterativeDeserializer::parseStreamHeader() {
    // TODO: Parse Java serialization stream header
    // Magic: 0xAC 0xED
    // Version: 0x00 0x05
    if (streamSize < 4) {
        throw std::runtime_error("Stream too short for header");
    }
    
    if (stream[0] != 0xAC || stream[1] != 0xED) {
        throw std::runtime_error("Invalid Java serialization magic number");
    }
    
    if (stream[2] != 0x00 || stream[3] != 0x05) {
        throw std::runtime_error("Unsupported Java serialization version");
    }
    
    streamPos = 4;
}

size_t IterativeDeserializer::createNode() {
    auto node = std::make_unique<IterativeDeserializer::DeserializationNode>();
    size_t index = nodes.size();
    nodes.push_back(std::move(node));
    return index;
}

// Stream parsing utilities
StreamTag IterativeDeserializer::readTag(size_t offset) {
    if (offset >= streamSize) {
        throw std::runtime_error("Stream read beyond end");
    }
    return static_cast<StreamTag>(stream[offset]);
}

uint8_t IterativeDeserializer::readByte(size_t offset) {
    if (offset >= streamSize) {
        throw std::runtime_error("Stream read beyond end");
    }
    return stream[offset];
}

uint16_t IterativeDeserializer::readShort(size_t offset) {
    if (offset + 1 >= streamSize) {
        throw std::runtime_error("Stream read beyond end");
    }
    return (static_cast<uint16_t>(stream[offset]) << 8) | stream[offset + 1];
}

uint32_t IterativeDeserializer::readInt(size_t offset) {
    if (offset + 3 >= streamSize) {
        throw std::runtime_error("Stream read beyond end");
    }
    return (static_cast<uint32_t>(stream[offset]) << 24) |
           (static_cast<uint32_t>(stream[offset + 1]) << 16) |
           (static_cast<uint32_t>(stream[offset + 2]) << 8) |
           stream[offset + 3];
}

uint64_t IterativeDeserializer::readLong(size_t offset) {
    if (offset + 7 >= streamSize) {
        throw std::runtime_error("Stream read beyond end");
    }
    uint64_t high = readInt(offset);
    uint64_t low = readInt(offset + 4);
    return (high << 32) | low;
}

std::string IterativeDeserializer::readString(size_t offset) {
    // offset should point to the tag byte
    StreamTag tag = readTag(offset);
    offset++;
    
    if (tag == StreamTag::TC_STRING) {
        uint16_t length = readShort(offset);
        offset += 2;
        
        if (offset + length > streamSize) {
            throw std::runtime_error("String read beyond end");
        }
        
        return std::string(reinterpret_cast<const char*>(stream + offset), length);
    } else if (tag == StreamTag::TC_LONGSTRING) {
        uint64_t length = readLong(offset);
        offset += 8;
        
        if (offset + length > streamSize) {
            throw std::runtime_error("Long string read beyond end");
        }
        
        return std::string(reinterpret_cast<const char*>(stream + offset), length);
    } else if (tag == StreamTag::TC_REFERENCE) {
        // String reference - return empty for now, will resolve later
        return "";
    } else {
        throw std::runtime_error("Expected string tag, got " + std::to_string(static_cast<int>(tag)));
    }
}

size_t IterativeDeserializer::readHandle(size_t offset) {
    StreamTag tag = readTag(offset);
    if (tag != StreamTag::TC_REFERENCE) {
        throw std::runtime_error("Expected reference tag");
    }
    offset++;
    return readInt(offset);
}

void IterativeDeserializer::skipBlockData(size_t& offset) {
    while (offset < streamSize) {
        StreamTag tag = readTag(offset);
        if (tag == StreamTag::TC_ENDBLOCKDATA) {
            offset++;
            break;
        } else if (tag == StreamTag::TC_BLOCKDATA) {
            offset++;
            uint8_t length = readByte(offset);
            offset++;
            offset += length;
        } else {
            break;
        }
    }
}

size_t IterativeDeserializer::getOrCreateNodeForHandle(size_t handleId) {
    auto it = handleToNode.find(handleId);
    if (it != handleToNode.end()) {
        return it->second;
    }
    
    size_t nodeIndex = createNode();
    nodes[nodeIndex]->handleId = handleId;
    handleToNode[handleId] = nodeIndex;
    return nodeIndex;
}

void IterativeDeserializer::processNode(const DeserializationTask& task) {
    if (task.nodeIndex >= nodes.size()) {
        throw std::runtime_error("Invalid node index");
    }
    
    size_t offset = task.streamOffset;
    if (offset >= streamSize) {
        throw std::runtime_error("Stream offset beyond end");
    }
    
    StreamTag tag = readTag(offset);
    offset++;
    
    // Check if this is a reference to an already-processed object
    if (tag == StreamTag::TC_REFERENCE) {
        size_t handleId = readInt(offset);
        size_t referencedNode = getOrCreateNodeForHandle(handleId);
        nodes[task.nodeIndex]->references.push_back(referencedNode);
        return;
    }
    
    // Process based on tag type
    if (tag == StreamTag::TC_OBJECT) {
        processObject(task.nodeIndex, offset);
    } else if (tag == StreamTag::TC_ARRAY) {
        processArray(task.nodeIndex, offset);
    } else if (tag == StreamTag::TC_STRING || tag == StreamTag::TC_LONGSTRING) {
        processString(task.nodeIndex, offset - 1);  // Include tag byte
    } else if (tag == StreamTag::TC_NULL) {
        // Null object, nothing to do
    } else {
        throw std::runtime_error("Unsupported stream tag");
    }
}

void IterativeDeserializer::processObject(size_t nodeIndex, size_t offset) {
    // TC_OBJECT (0x73) followed by:
    // - Class descriptor (TC_CLASSDESC or TC_REFERENCE)
    // - Class data (fields)
    
    // Read class descriptor
    StreamTag classTag = readTag(offset);
    offset++;
    
    if (classTag == StreamTag::TC_CLASSDESC) {
        processClassDesc(offset);
        // Skip class descriptor data for now
        // TODO: Parse class descriptor fully
    } else if (classTag == StreamTag::TC_REFERENCE) {
        size_t handleId = readInt(offset);
        // Reference to existing class descriptor
    } else {
        throw std::runtime_error("Expected class descriptor");
    }
    
    // For now, create a placeholder object
    // TODO: Parse actual object data based on class descriptor
    auto objData = std::make_unique<uint8_t[]>(1);
    objData[0] = 0;  // Placeholder
    nodes[nodeIndex]->object = ObjectGraphPtr(objData.release(), ObjectGraphDeleter());
    
    // Assign handle ID
    nodes[nodeIndex]->handleId = nextHandleId++;
    handleToNode[nodes[nodeIndex]->handleId] = nodeIndex;
}

void IterativeDeserializer::processClassDesc(size_t offset) {
    // TC_CLASSDESC structure:
    // - Class name (TC_STRING)
    // - Serial version UID (long)
    // - Flags (byte)
    // - Field count (short)
    // - Fields...
    // - Class annotations (TC_ENDBLOCKDATA)
    // - Super class descriptor
    
    // Skip for now - just read basic structure
    // TODO: Parse fully to understand object layout
}

void IterativeDeserializer::processArray(size_t nodeIndex, size_t offset) {
    // TC_ARRAY (0x75) followed by:
    // - Class descriptor for array type
    // - Array length (int)
    // - Array elements
    
    // Read class descriptor
    StreamTag classTag = readTag(offset);
    offset++;
    
    if (classTag == StreamTag::TC_CLASSDESC) {
        processClassDesc(offset);
    }
    
    // Read array length
    uint32_t length = readInt(offset);
    offset += 4;
    
    // For now, create placeholder
    // TODO: Parse array elements based on type
    auto arrData = std::make_unique<uint8_t[]>(1);
    arrData[0] = 0;
    nodes[nodeIndex]->object = ObjectGraphPtr(arrData.release(), ObjectGraphDeleter());
    
    nodes[nodeIndex]->handleId = nextHandleId++;
    handleToNode[nodes[nodeIndex]->handleId] = nodeIndex;
}

void IterativeDeserializer::processString(size_t nodeIndex, size_t offset) {
    StreamTag tag = readTag(offset);
    
    if (tag == StreamTag::TC_REFERENCE) {
        // String reference - resolve to existing string
        size_t handleId = readInt(offset + 1);
        size_t referencedNode = getOrCreateNodeForHandle(handleId);
        nodes[nodeIndex]->references.push_back(referencedNode);
        return;
    }
    
    std::string str = readString(offset);
    
    if (str.empty() && tag == StreamTag::TC_REFERENCE) {
        // Will be resolved in resolveReferences
        return;
    }
    
    // Allocate memory for string
    size_t strSize = str.length() + 1;
    auto strData = std::make_unique<uint8_t[]>(strSize);
    std::memcpy(strData.get(), str.c_str(), strSize);
    
    nodes[nodeIndex]->object = ObjectGraphPtr(strData.release(), ObjectGraphDeleter());
    
    nodes[nodeIndex]->handleId = nextHandleId++;
    handleToNode[nodes[nodeIndex]->handleId] = nodeIndex;
}

void IterativeDeserializer::processReferences(size_t nodeIndex) {
    // TODO: Scan object data for references
    // For now, references are collected during processNode
    // This method will be expanded to scan object fields for references
}

void IterativeDeserializer::resolveReferences() {
    // Resolve all references between nodes
    // For now, references are already linked in nodes[].references
    // This method will handle circular reference detection and linking
    // TODO: Implement full reference resolution
}

