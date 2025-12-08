#include "deepcopy.h"
#include "deserializer.h"
#include <cstring>
#include <string>
#include <memory>
#include <unordered_map>

// Store active object graphs with smart pointers
// When freeDeepCopy() is called, we release the unique_ptr
static thread_local std::unordered_map<void*, IterativeDeserializer::ObjectGraphPtr> activeGraphs;
static thread_local std::string lastError;

void* deepCopyFromBuffer(const void* serializedData, size_t dataSize) {
    try {
        if (!serializedData || dataSize == 0) {
            lastError = "Invalid input: null pointer or zero size";
            return nullptr;
        }
        
        // Validate stream header
        const uint8_t* bytes = static_cast<const uint8_t*>(serializedData);
        if (dataSize < 6) {
            lastError = "Stream too short: " + std::to_string(dataSize) + " bytes (need at least 6)";
            return nullptr;
        }
        
        if (bytes[0] != 0xAC || bytes[1] != 0xED) {
            lastError = "Invalid Java serialization magic: expected 0xAC 0xED, got " +
                       std::to_string(bytes[0]) + " " + std::to_string(bytes[1]);
            return nullptr;
        }
        
        IterativeDeserializer deserializer;
        
        // Deserialize returns unique_ptr (automatic memory management)
        auto result = deserializer.deserialize(serializedData, dataSize);
        
        if (!result) {
            lastError = deserializer.getLastError();
            if (lastError.empty()) {
                lastError = "Deserialization failed";
            }
            return nullptr;
        }
        
        // Get raw pointer for C interface
        void* rawPtr = result.get();
        
        // Store unique_ptr in map (transfers ownership)
        // When freeDeepCopy() is called, we remove from map and unique_ptr destructs
        activeGraphs[rawPtr] = std::move(result);
        
        return rawPtr;
        
    } catch (const std::exception& e) {
        lastError = std::string("Exception: ") + e.what();
        return nullptr;
    } catch (...) {
        lastError = "Unknown exception";
        return nullptr;
    }
}

void freeDeepCopy(void* copiedObj) {
    if (copiedObj) {
        // Find and remove from map
        // unique_ptr destructor automatically frees object graph via custom deleter
        auto it = activeGraphs.find(copiedObj);
        if (it != activeGraphs.end()) {
            activeGraphs.erase(it);
            // ObjectGraphDeleter automatically called here
        }
    }
}

const char* getLastError() {
    return lastError.c_str();
}

