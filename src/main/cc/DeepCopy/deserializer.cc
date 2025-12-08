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
    classDescriptors.clear();
    handleToString.clear();
    
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
        
        std::string str(reinterpret_cast<const char*>(stream + offset), length);
        
        // Assign handle to this string
        size_t stringHandle = nextHandleId++;
        handleToString[stringHandle] = str;
        
        return str;
    } else if (tag == StreamTag::TC_LONGSTRING) {
        uint64_t length = readLong(offset);
        offset += 8;
        
        if (offset + length > streamSize) {
            throw std::runtime_error("Long string read beyond end");
        }
        
        std::string str(reinterpret_cast<const char*>(stream + offset), length);
        
        // Assign handle to this string
        size_t stringHandle = nextHandleId++;
        handleToString[stringHandle] = str;
        
        return str;
    } else if (tag == StreamTag::TC_REFERENCE) {
        // String reference - look up by handle
        size_t stringHandle = readInt(offset);
        auto it = handleToString.find(stringHandle);
        if (it == handleToString.end()) {
            throw std::runtime_error("String reference not found: " + std::to_string(stringHandle));
        }
        return it->second;
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

size_t IterativeDeserializer::skipObjectData(size_t offset) {
    // offset should point to the tag byte
    StreamTag tag = readTag(offset);
    offset++;
    
    if (tag == StreamTag::TC_NULL) {
        // Null - already skipped tag
        return offset;
    } else if (tag == StreamTag::TC_REFERENCE) {
        // Reference - skip handle (4 bytes)
        return offset + 4;
    } else if (tag == StreamTag::TC_STRING) {
        // String - skip length (2 bytes) + data
        uint16_t length = readShort(offset);
        return offset + 2 + length;
    } else if (tag == StreamTag::TC_LONGSTRING) {
        // Long string - skip length (8 bytes) + data
        uint64_t length = readLong(offset);
        return offset + 8 + length;
    } else if (tag == StreamTag::TC_OBJECT) {
        // Object - skip class descriptor and object data
        StreamTag classTag = readTag(offset);
        offset++;
        
        size_t classDescHandleId = 0;
        const ClassDescriptor* desc = nullptr;
        
        if (classTag == StreamTag::TC_CLASSDESC) {
            // Parse class descriptor
            size_t classDescEnd = processClassDesc(offset);
            classDescHandleId = nextHandleId - 1;  // Last assigned handle
            offset = classDescEnd;
        } else if (classTag == StreamTag::TC_REFERENCE) {
            // Reference to existing class descriptor
            classDescHandleId = readInt(offset);
            offset += 4;
        } else {
            throw std::runtime_error("Expected class descriptor in object");
        }
        
        // Get class descriptor
        auto it = classDescriptors.find(classDescHandleId);
        if (it == classDescriptors.end()) {
            throw std::runtime_error("Class descriptor not found");
        }
        desc = &it->second;
        
        // Skip object fields based on class descriptor
        // Primitives first
        for (const auto& field : desc->fields) {
            switch (field.typeCode) {
                case 'B': offset += 1; break;
                case 'C': offset += 2; break;
                case 'D': offset += 8; break;
                case 'F': offset += 4; break;
                case 'I': offset += 4; break;
                case 'J': offset += 8; break;
                case 'S': offset += 2; break;
                case 'Z': offset += 1; break;
                case 'L':
                case '[':
                    // Object/array field - recursively skip
                    offset = skipObjectData(offset);
                    break;
            }
        }
        
        return offset;
    } else if (tag == StreamTag::TC_ARRAY) {
        // Array - skip class descriptor, length, and elements
        StreamTag arrayClassTag = readTag(offset);
        offset++;
        
        if (arrayClassTag == StreamTag::TC_CLASSDESC) {
            offset = processClassDesc(offset);
        } else if (arrayClassTag == StreamTag::TC_REFERENCE) {
            offset += 4;
        } else {
            throw std::runtime_error("Expected class descriptor in array");
        }
        
        // Read array length
        uint32_t length = readInt(offset);
        offset += 4;
        
        // For now, arrays are not fully supported in skip
        // This will be handled properly in processArray
        // TODO: Implement proper array element skipping
        throw std::runtime_error("Array skipping not fully implemented");
    } else {
        throw std::runtime_error("Unsupported tag in skipObjectData: " + std::to_string(static_cast<int>(tag)));
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
    // Note: offset points to the byte AFTER TC_OBJECT tag
    
    // Read class descriptor tag
    StreamTag classTag = readTag(offset);
    offset++;  // Skip class descriptor tag
    
    size_t classDescHandleId = 0;
    size_t dataOffset;
    
    if (classTag == StreamTag::TC_CLASSDESC) {
        size_t classDescEnd = processClassDesc(offset);
        // Get handle ID of the class descriptor we just created
        classDescHandleId = nextHandleId - 1;  // Last assigned handle
        dataOffset = classDescEnd;
    } else if (classTag == StreamTag::TC_REFERENCE) {
        classDescHandleId = readInt(offset);
        dataOffset = offset + 4;  // Handle (4 bytes)
        // Reference to existing class descriptor
    } else {
        throw std::runtime_error("Expected class descriptor, got tag: " + 
                                std::to_string(static_cast<int>(classTag)) + 
                                " at offset " + std::to_string(offset - 1));
    }
    
    // Get class descriptor to parse object data
    auto it = classDescriptors.find(classDescHandleId);
    if (it == classDescriptors.end()) {
        throw std::runtime_error("Class descriptor not found");
    }
    const ClassDescriptor& desc = it->second;
    
    // Parse object fields based on class descriptor
    // Fields are written in declaration order: primitives first, then objects
    size_t currentOffset = dataOffset;
    
    // First pass: read all primitive fields
    for (const auto& field : desc.fields) {
        switch (field.typeCode) {
            case 'B':  // byte
                readByte(currentOffset);
                currentOffset += 1;
                break;
            case 'C':  // char (UTF-16, 2 bytes)
                readShort(currentOffset);
                currentOffset += 2;
                break;
            case 'D':  // double
                readLong(currentOffset);  // Read as 8 bytes
                currentOffset += 8;
                break;
            case 'F':  // float
                readInt(currentOffset);  // Read as 4 bytes
                currentOffset += 4;
                break;
            case 'I':  // int
                readInt(currentOffset);
                currentOffset += 4;
                break;
            case 'J':  // long
                readLong(currentOffset);
                currentOffset += 8;
                break;
            case 'S':  // short
                readShort(currentOffset);
                currentOffset += 2;
                break;
            case 'Z':  // boolean
                readByte(currentOffset);
                currentOffset += 1;
                break;
            case 'L':  // Object type - will be handled in second pass
            case '[':  // Array type - will be handled in second pass
                // Skip for now, handle in second pass
                break;
            default:
                throw std::runtime_error("Unsupported field type code");
        }
    }
    
    // Second pass: read object and array fields
    for (const auto& field : desc.fields) {
        if (field.typeCode == 'L' || field.typeCode == '[') {
            // Object or array field - read stream tag
            StreamTag tag = readTag(currentOffset);
            currentOffset++;
            
            if (tag == StreamTag::TC_NULL) {
                // Null reference - nothing to do
            } else if (tag == StreamTag::TC_REFERENCE) {
                // Reference to existing object
                size_t handleId = readInt(currentOffset);
                currentOffset += 4;
                
                // Create or get node for this reference
                size_t refNodeIndex = getOrCreateNodeForHandle(handleId);
                nodes[nodeIndex]->references.push_back(refNodeIndex);
                nodes[nodeIndex]->referenceOffsets.push_back(currentOffset - 5);  // Store offset where reference was found
            } else if (tag == StreamTag::TC_OBJECT || tag == StreamTag::TC_ARRAY || 
                       tag == StreamTag::TC_STRING || tag == StreamTag::TC_LONGSTRING) {
                // New object/array/string - create node and add to work queue
                size_t refNodeIndex = createNode();
                nodes[nodeIndex]->references.push_back(refNodeIndex);
                nodes[nodeIndex]->referenceOffsets.push_back(currentOffset - 1);  // Store offset (including tag)
                
                // Add to work queue for processing
                workQueue.push({refNodeIndex, currentOffset - 1});
                
                // Skip over the object/array/string to continue parsing remaining fields
                // We need to parse enough to find where it ends
                currentOffset = skipObjectData(currentOffset - 1);  // Include tag byte
            } else {
                throw std::runtime_error("Unexpected tag in object field");
            }
        }
    }
    
    // Store object data offset for later reference resolution
    // For now, create a minimal object representation
    auto objData = std::make_unique<uint8_t[]>(sizeof(size_t));
    *reinterpret_cast<size_t*>(objData.get()) = dataOffset;  // Store offset to object data
    nodes[nodeIndex]->object = ObjectGraphPtr(objData.release(), ObjectGraphDeleter());
    nodes[nodeIndex]->classId = classDescHandleId;
    
    // Assign handle ID
    nodes[nodeIndex]->handleId = nextHandleId++;
    handleToNode[nodes[nodeIndex]->handleId] = nodeIndex;
}

size_t IterativeDeserializer::processClassDesc(size_t offset) {
    // TC_CLASSDESC structure:
    // - Class name (TC_STRING or TC_REFERENCE)
    // - Serial version UID (long)
    // - Flags (byte)
    // - Field count (short)
    // - Fields (type code + field name for each)
    // - Class annotations (TC_ENDBLOCKDATA)
    // - Super class descriptor (TC_NULL or TC_CLASSDESC)
    // Note: offset points to the byte AFTER TC_CLASSDESC tag
    
    ClassDescriptor desc;
    size_t currentOffset = offset;
    
    // Assign handle to this class descriptor BEFORE processing
    size_t classDescHandle = nextHandleId++;
    
    // Read class name (can be TC_STRING or TC_REFERENCE)
    StreamTag nameTag = readTag(currentOffset);
    currentOffset++;  // Skip tag byte
    
    if (nameTag == StreamTag::TC_STRING) {
        uint16_t length = readShort(currentOffset);
        currentOffset += 2;
        
        if (currentOffset + length > streamSize) {
            throw std::runtime_error("Class name string read beyond end");
        }
        
        desc.className = std::string(reinterpret_cast<const char*>(stream + currentOffset), length);
        currentOffset += length;
        
        // Assign handle to this string
        size_t stringHandle = nextHandleId++;
        handleToString[stringHandle] = desc.className;
    } else if (nameTag == StreamTag::TC_REFERENCE) {
        // Reference to previously seen string
        size_t stringHandle = readInt(currentOffset);
        currentOffset += 4;  // Handle (4 bytes)
        
        // Look up string by handle
        auto it = handleToString.find(stringHandle);
        if (it == handleToString.end()) {
            throw std::runtime_error("String reference not found: " + std::to_string(stringHandle));
        }
        desc.className = it->second;
    } else {
        throw std::runtime_error("Expected string or reference for class name, got tag: " + 
                                std::to_string(static_cast<int>(nameTag)) + " at offset " + 
                                std::to_string(currentOffset - 1));
    }
    
    // Read serial version UID (long, 8 bytes)
    desc.serialVersionUID = readLong(currentOffset);
    currentOffset += 8;
    
    // Read flags (byte)
    desc.flags = readByte(currentOffset);
    currentOffset += 1;
    
    // Read field count (short)
    uint16_t fieldCount = readShort(currentOffset);
    currentOffset += 2;
    
    // Read fields
    for (uint16_t i = 0; i < fieldCount; i++) {
        FieldDescriptor field;
        
        // Read type code (byte)
        field.typeCode = static_cast<char>(readByte(currentOffset));
        currentOffset += 1;
        
        // For object types ('L'), read type name
        if (field.typeCode == 'L') {
            StreamTag typeTag = readTag(currentOffset);
            currentOffset++;
            if (typeTag == StreamTag::TC_STRING) {
                uint16_t length = readShort(currentOffset);
                currentOffset += 2;
                if (currentOffset + length > streamSize) {
                    throw std::runtime_error("Type name string read beyond end");
                }
                field.typeName = std::string(reinterpret_cast<const char*>(stream + currentOffset), length);
                currentOffset += length;
                
                // Assign handle
                size_t stringHandle = nextHandleId++;
                handleToString[stringHandle] = field.typeName;
            } else if (typeTag == StreamTag::TC_REFERENCE) {
                size_t stringHandle = readInt(currentOffset);
                currentOffset += 4;
                auto it = handleToString.find(stringHandle);
                if (it == handleToString.end()) {
                    throw std::runtime_error("Type name reference not found: " + std::to_string(stringHandle));
                }
                field.typeName = it->second;
            } else {
                throw std::runtime_error("Expected string or reference for object type name");
            }
        }
        
        // Read field name (TC_STRING or TC_REFERENCE)
        StreamTag fieldNameTag = readTag(currentOffset);
        currentOffset++;
        if (fieldNameTag == StreamTag::TC_STRING) {
            uint16_t length = readShort(currentOffset);
            currentOffset += 2;
            if (currentOffset + length > streamSize) {
                throw std::runtime_error("Field name string read beyond end");
            }
            field.fieldName = std::string(reinterpret_cast<const char*>(stream + currentOffset), length);
            currentOffset += length;
            
            // Assign handle
            size_t stringHandle = nextHandleId++;
            handleToString[stringHandle] = field.fieldName;
        } else if (fieldNameTag == StreamTag::TC_REFERENCE) {
            size_t stringHandle = readInt(currentOffset);
            currentOffset += 4;
            auto it = handleToString.find(stringHandle);
            if (it == handleToString.end()) {
                throw std::runtime_error("Field name reference not found: " + std::to_string(stringHandle));
            }
            field.fieldName = it->second;
        } else {
            throw std::runtime_error("Expected string or reference for field name");
        }
        
        desc.fields.push_back(field);
    }
    
    // Skip class annotations (TC_ENDBLOCKDATA)
    StreamTag endTag = readTag(currentOffset);
    if (endTag == StreamTag::TC_ENDBLOCKDATA) {
        currentOffset += 1;
    } else {
        // May have block data before ENDBLOCKDATA
        skipBlockData(currentOffset);
    }
    
    // Read super class descriptor (TC_NULL or TC_CLASSDESC)
    StreamTag superTag = readTag(currentOffset);
    if (superTag == StreamTag::TC_CLASSDESC) {
        currentOffset += 1;
        size_t superHandleId = processClassDesc(currentOffset);
        // Track super class relationship if needed
    } else if (superTag == StreamTag::TC_NULL) {
        currentOffset += 1;
    } else if (superTag == StreamTag::TC_REFERENCE) {
        currentOffset += 1;
        size_t superHandleId = readInt(currentOffset);
        currentOffset += 4;
        // Reference to existing class descriptor
    }
    
    // Assign handle ID and store
    desc.handleId = nextHandleId++;
    classDescriptors[desc.handleId] = desc;
    
    // Update stream position (if tracking globally)
    // For now, return the offset after class descriptor
    return currentOffset;
}

void IterativeDeserializer::processArray(size_t nodeIndex, size_t offset) {
    // TC_ARRAY (0x75) followed by:
    // - Class descriptor for array type
    // - Array length (int)
    // - Array elements
    
    // Read class descriptor
    StreamTag classTag = readTag(offset);
    offset++;
    
    size_t arrayTypeOffset = offset;
    if (classTag == StreamTag::TC_CLASSDESC) {
        arrayTypeOffset = processClassDesc(offset);
    } else if (classTag == StreamTag::TC_REFERENCE) {
        size_t handleId = readInt(offset);
        arrayTypeOffset = offset + 4;
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
    // Verify all references are valid and handle circular references
    for (size_t i = 0; i < nodes.size(); i++) {
        if (!nodes[i]) continue;
        
        // Verify all referenced nodes exist
        for (size_t refIndex : nodes[i]->references) {
            if (refIndex >= nodes.size() || !nodes[refIndex]) {
                throw std::runtime_error("Invalid reference: node " + std::to_string(i) + 
                                         " references non-existent node " + std::to_string(refIndex));
            }
        }
        
        // Circular references are already handled by the reference tracking
        // Each node can reference any other node, including itself or nodes that reference back
    }
}

