#ifndef DEEPCOPY_H
#define DEEPCOPY_H

#include <cstddef>

/**
 * C interface for deep copying Java objects from serialized buffers.
 * 
 * This interface is designed to be called from Java via JNA.
 * Java serializes objects to buffers, then C++ deserializes iteratively
 * to avoid stack overflow issues with deep object graphs.
 */

extern "C" {
    /**
     * Deep copy Java object from serialized buffer.
     * 
     * @param serializedData Pointer to Java serialization stream
     * @param dataSize Size of serialized data in bytes
     * @return Opaque pointer to copied object graph, or nullptr on error
     * @note Caller must call freeDeepCopy() to release memory
     */
    void* deepCopyFromBuffer(const void* serializedData, size_t dataSize);
    
    /**
     * Free memory allocated by deepCopyFromBuffer.
     * 
     * @param copiedObj Pointer returned from deepCopyFromBuffer
     * @note Safe to call with nullptr
     */
    void freeDeepCopy(void* copiedObj);
    
    /**
     * Get error message if last operation failed.
     * 
     * @return Error message string, or nullptr if no error
     * @note String is valid until next call to deepCopyFromBuffer
     */
    const char* getLastError();
}

#endif

