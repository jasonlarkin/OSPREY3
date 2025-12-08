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
        // Push offset to the tag byte since processNode will read the tag
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
            size_t classDescEnd = processClassDesc(offset, classDescHandleId);
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
                    throw std::runtime_error("Class descriptor not found for handle: " + 
                                            std::to_string(classDescHandleId) + 
                                            " at offset " + std::to_string(offset - 1));
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
        
        size_t classDescHandleId = 0;
        if (arrayClassTag == StreamTag::TC_CLASSDESC) {
            // Parse class descriptor - it will be stored with handle ID assigned by processClassDesc
            offset = processClassDesc(offset, classDescHandleId);
        } else if (arrayClassTag == StreamTag::TC_REFERENCE) {
            classDescHandleId = readInt(offset);
            offset += 4;
        } else {
            throw std::runtime_error("Expected class descriptor in array");
        }
        
        // Get class descriptor to determine element type
        // Note: For TC_REFERENCE, the class descriptor must have been parsed earlier
        auto it = classDescriptors.find(classDescHandleId);
        if (it == classDescriptors.end()) {
            // If not found, it might not have been parsed yet (forward reference)
            // This can happen when skipping nested arrays
            // We need to determine element type heuristically by peeking at the first element
            uint32_t length = readInt(offset);
            offset += 4;
            
            if (length == 0) {
                // Empty array - nothing to skip
                return offset;
            }
            
            // Peek at first element to determine if it's primitive or object
            // If it's a valid StreamTag (0x70-0x7E), it's an object/array/string/null/reference
            // Otherwise, it's likely a primitive
            uint8_t firstByte = readByte(offset);
            bool isPrimitive = (firstByte < 0x70 || firstByte > 0x7E);
            
            if (isPrimitive) {
                // Primitive array - we can't determine exact type, so we'll skip based on common sizes
                // This is a heuristic: try 4 bytes (int/float) first, fall back to other sizes if needed
                // For now, assume int[] (4 bytes per element) as most common
                offset += length * 4;
            } else {
                // Object array - skip elements by reading tags
                for (uint32_t i = 0; i < length; i++) {
                    StreamTag elementTag = readTag(offset);
                    offset++;
                    if (elementTag == StreamTag::TC_NULL) {
                        // Null element - already skipped
                    } else if (elementTag == StreamTag::TC_REFERENCE) {
                        // Reference - skip handle
                        offset += 4;
                    } else {
                        // Object/array/string - recursively skip
                        offset = skipObjectData(offset - 1);
                    }
                }
            }
            return offset;
        }
        const ClassDescriptor& desc = it->second;
        
        // Determine element type from class name
        if (desc.className.empty() || desc.className[0] != '[') {
            throw std::runtime_error("Invalid array class name: " + desc.className);
        }
        char elementType = desc.className[1];
        
        // Read array length
        uint32_t length = readInt(offset);
        offset += 4;
        
        // Skip array elements based on element type
        for (uint32_t i = 0; i < length; i++) {
            if (elementType == 'L' || elementType == '[') {
                // Object or array element - read tag first, then skip
                StreamTag elementTag = readTag(offset);
                offset++;
                if (elementTag == StreamTag::TC_NULL) {
                    // Null element - already skipped tag, nothing more to skip
                } else if (elementTag == StreamTag::TC_REFERENCE) {
                    // Reference - skip handle (4 bytes)
                    offset += 4;
                } else {
                    // Object/array/string - recursively skip (offset-1 to include tag)
                    offset = skipObjectData(offset - 1);
                }
            } else {
                // Primitive element - skip based on type
                switch (elementType) {
                    case 'B': offset += 1; break;  // byte
                    case 'C': offset += 2; break;  // char
                    case 'D': offset += 8; break;  // double
                    case 'F': offset += 4; break;  // float
                    case 'I': offset += 4; break;  // int
                    case 'J': offset += 8; break;  // long
                    case 'S': offset += 2; break;  // short
                    case 'Z': offset += 1; break;  // boolean
                    default:
                        throw std::runtime_error("Unsupported array element type: " + std::string(1, elementType));
                }
            }
        }
        
        return offset;
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
        size_t classDescEnd = processClassDesc(offset, classDescHandleId);
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
        throw std::runtime_error("Class descriptor not found for handle: " + 
                                std::to_string(classDescHandleId) + 
                                " at offset " + std::to_string(offset - 1));
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
            } else if (tag == StreamTag::TC_BLOCKDATA || tag == StreamTag::TC_ENDBLOCKDATA) {
                // Block data for custom serialization - skip it
                // TC_BLOCKDATA is followed by length (1 byte) and data
                // TC_ENDBLOCKDATA is just a marker (1 byte)
                if (tag == StreamTag::TC_BLOCKDATA) {
                    uint8_t length = readByte(currentOffset);
                    currentOffset += 1 + length;  // Skip length byte and data
                } else {
                    // TC_ENDBLOCKDATA - just skip the tag (already incremented)
                }
            } else {
                throw std::runtime_error("Unexpected tag in object field: " + 
                                        std::to_string(static_cast<int>(tag)) + 
                                        " at offset " + std::to_string(currentOffset - 1));
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

size_t IterativeDeserializer::processClassDesc(size_t offset, size_t& handleId) {
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
    
    // Read class name FIRST (strings get handles before class descriptors in Java serialization)
    // In TC_CLASSDESC, class name is written as "newString" which can be:
    // 1. TC_STRING (0x74) + length (2 bytes) + data
    // 2. TC_REFERENCE (0x71) + handle (4 bytes)
    // 3. Directly as length (2 bytes) + data (when written inline in class descriptor)
    // Check if next byte is a tag or length
    StreamTag nameTag = readTag(currentOffset);
    
    if (nameTag == StreamTag::TC_STRING) {
        // TC_STRING tag present
        currentOffset++;  // Skip tag byte
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
        currentOffset++;  // Skip tag byte
        size_t stringHandle = readInt(currentOffset);
        currentOffset += 4;  // Handle (4 bytes)
        
        // Look up string by handle
        auto it = handleToString.find(stringHandle);
        if (it == handleToString.end()) {
            throw std::runtime_error("String reference not found: " + std::to_string(stringHandle));
        }
        desc.className = it->second;
    } else {
        // No tag - class name is written directly as length + data (newString format in TC_CLASSDESC)
        // In this case, the string does NOT get a handle - only the class descriptor gets a handle
        uint16_t length = readShort(currentOffset);
        currentOffset += 2;
        
        if (currentOffset + length > streamSize) {
            throw std::runtime_error("Class name string read beyond end");
        }
        
        desc.className = std::string(reinterpret_cast<const char*>(stream + currentOffset), length);
        currentOffset += length;
        
        // Do NOT assign handle to this string - when written directly in TC_CLASSDESC,
        // only the class descriptor itself gets a handle
    }
    
    // Assign handle ID to class descriptor
    // If class name had TC_STRING tag, string got handle first, then descriptor
    // If class name was written directly, descriptor gets the handle
    desc.handleId = nextHandleId++;
    // Store immediately (with just handleId) so references can find it
    // We'll update it with complete data at the end
    classDescriptors[desc.handleId] = desc;
    
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
        
        // For object types ('L'), read type name (newString format)
        if (field.typeCode == 'L') {
            StreamTag typeTag = readTag(currentOffset);
            if (typeTag == StreamTag::TC_STRING) {
                currentOffset++;  // Skip tag byte
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
                currentOffset++;  // Skip tag byte
                size_t stringHandle = readInt(currentOffset);
                currentOffset += 4;
                auto it = handleToString.find(stringHandle);
                if (it == handleToString.end()) {
                    throw std::runtime_error("Type name reference not found: " + std::to_string(stringHandle));
                }
                field.typeName = it->second;
            } else {
                // No tag - type name is written directly as length + data (newString format)
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
            }
        }
        
        // Read field name (newString format: TC_STRING, TC_REFERENCE, or direct length+data)
        StreamTag fieldNameTag = readTag(currentOffset);
        if (fieldNameTag == StreamTag::TC_STRING) {
            currentOffset++;  // Skip tag byte
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
            currentOffset++;  // Skip tag byte
            size_t stringHandle = readInt(currentOffset);
            currentOffset += 4;
            auto it = handleToString.find(stringHandle);
            if (it == handleToString.end()) {
                throw std::runtime_error("Field name reference not found: " + std::to_string(stringHandle));
            }
            field.fieldName = it->second;
        } else {
            // No tag - field name is written directly as length + data (newString format)
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
        }
        
        desc.fields.push_back(field);
    }
    
    // Skip class annotations (TC_ENDBLOCKDATA)
    // After fields, there should be TC_ENDBLOCKDATA (or TC_BLOCKDATA followed by TC_ENDBLOCKDATA)
    StreamTag endTag = readTag(currentOffset);
    if (endTag == StreamTag::TC_ENDBLOCKDATA) {
        currentOffset += 1;
        } else if (endTag == StreamTag::TC_BLOCKDATA) {
            // Block data before ENDBLOCKDATA
            skipBlockData(currentOffset);
        } else {
            // Unexpected tag - this might indicate a parsing error
            // But for arrays with 0 fields, we should have TC_ENDBLOCKDATA here
            throw std::runtime_error("Expected TC_ENDBLOCKDATA after fields, got tag: " + 
                                std::to_string(static_cast<int>(endTag)) + 
                                " at offset " + std::to_string(currentOffset));
        }
        
        // Read super class descriptor (TC_NULL or TC_CLASSDESC)
        StreamTag superTag = readTag(currentOffset);
        if (superTag == StreamTag::TC_CLASSDESC) {
            currentOffset += 1;
            size_t superHandleId;
            currentOffset = processClassDesc(currentOffset, superHandleId);
        // Track super class relationship if needed
    } else if (superTag == StreamTag::TC_NULL) {
        currentOffset += 1;
    } else if (superTag == StreamTag::TC_REFERENCE) {
        currentOffset += 1;
        size_t superHandleId = readInt(currentOffset);
        currentOffset += 4;
        // Reference to existing class descriptor
    }
    
    // Handle ID already assigned and stored at the beginning
    // Update the stored descriptor with final complete data
    classDescriptors[desc.handleId] = desc;
    
    // Return the handle ID via reference parameter
    handleId = desc.handleId;
    
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
    
    size_t classDescHandleId = 0;
    size_t dataOffset;
    
    if (classTag == StreamTag::TC_CLASSDESC) {
        dataOffset = processClassDesc(offset, classDescHandleId);
    } else if (classTag == StreamTag::TC_REFERENCE) {
        classDescHandleId = readInt(offset);
        dataOffset = offset + 4;
    } else {
        throw std::runtime_error("Expected class descriptor in array");
    }
    
    // Get class descriptor to determine element type
    auto it = classDescriptors.find(classDescHandleId);
    if (it == classDescriptors.end()) {
        throw std::runtime_error("Array class descriptor not found for handle: " + 
                                std::to_string(classDescHandleId) + 
                                " at offset " + std::to_string(offset - 1));
    }
    const ClassDescriptor& desc = it->second;
    
    // Array class names: "[I" for int[], "[Ljava/lang/String;" for String[]
    // First character is '[', rest is element type
    if (desc.className.empty() || desc.className[0] != '[') {
        throw std::runtime_error("Invalid array class name: " + desc.className);
    }
    
    // Determine element type from class name
    char elementType = desc.className[1];
    
    // Read array length (at dataOffset, after class descriptor)
    uint32_t length = readInt(dataOffset);
    size_t currentOffset = dataOffset + 4;
    
    // Parse array elements based on element type
    for (uint32_t i = 0; i < length; i++) {
        if (elementType == 'L') {
            // Object array - elements are stream tags
            StreamTag tag = readTag(currentOffset);
            currentOffset++;
            
            if (tag == StreamTag::TC_NULL) {
                // Null element - nothing to do
            } else if (tag == StreamTag::TC_REFERENCE) {
                // Reference to existing object
                size_t handleId = readInt(currentOffset);
                currentOffset += 4;
                size_t refNodeIndex = getOrCreateNodeForHandle(handleId);
                nodes[nodeIndex]->references.push_back(refNodeIndex);
            } else if (tag == StreamTag::TC_OBJECT || tag == StreamTag::TC_ARRAY || 
                       tag == StreamTag::TC_STRING || tag == StreamTag::TC_LONGSTRING) {
                // New object/array/string element
                size_t refNodeIndex = createNode();
                nodes[nodeIndex]->references.push_back(refNodeIndex);
                workQueue.push({refNodeIndex, currentOffset - 1});
                currentOffset = skipObjectData(currentOffset - 1);
            } else {
                throw std::runtime_error("Unexpected tag in array element");
            }
        } else {
            // Primitive array - read elements directly
            switch (elementType) {
                case 'B':  // byte[]
                    readByte(currentOffset);
                    currentOffset += 1;
                    break;
                case 'C':  // char[]
                    readShort(currentOffset);
                    currentOffset += 2;
                    break;
                case 'D':  // double[]
                    readLong(currentOffset);
                    currentOffset += 8;
                    break;
                case 'F':  // float[]
                    readInt(currentOffset);
                    currentOffset += 4;
                    break;
                case 'I':  // int[]
                    readInt(currentOffset);
                    currentOffset += 4;
                    break;
                case 'J':  // long[]
                    readLong(currentOffset);
                    currentOffset += 8;
                    break;
                case 'S':  // short[]
                    readShort(currentOffset);
                    currentOffset += 2;
                    break;
                case 'Z':  // boolean[]
                    readByte(currentOffset);
                    currentOffset += 1;
                    break;
                case '[': {  // Multi-dimensional array
                    // Nested array - read tag first, then treat as object
                    StreamTag nestedTag = readTag(currentOffset);
                    currentOffset++;
                    if (nestedTag == StreamTag::TC_NULL) {
                        // Null element - nothing to do
                    } else if (nestedTag == StreamTag::TC_REFERENCE) {
                        // Reference to existing array
                        size_t handleId = readInt(currentOffset);
                        currentOffset += 4;
                        size_t refNodeIndex = getOrCreateNodeForHandle(handleId);
                        nodes[nodeIndex]->references.push_back(refNodeIndex);
                    } else if (nestedTag == StreamTag::TC_ARRAY) {
                        // New nested array
                        size_t refNodeIndex = createNode();
                        nodes[nodeIndex]->references.push_back(refNodeIndex);
                        workQueue.push({refNodeIndex, currentOffset - 1});
                        currentOffset = skipObjectData(currentOffset - 1);
                    } else {
                        throw std::runtime_error("Unexpected tag in nested array element");
                    }
                    break;
                }
                default:
                    throw std::runtime_error("Unsupported array element type: " + std::string(1, elementType));
            }
        }
    }
    
    // Store array data offset
    auto arrData = std::make_unique<uint8_t[]>(sizeof(size_t));
    *reinterpret_cast<size_t*>(arrData.get()) = dataOffset;
    nodes[nodeIndex]->object = ObjectGraphPtr(arrData.release(), ObjectGraphDeleter());
    nodes[nodeIndex]->classId = classDescHandleId;
    
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

