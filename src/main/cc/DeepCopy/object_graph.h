#ifndef OBJECT_GRAPH_H
#define OBJECT_GRAPH_H

#include <memory>
#include <vector>
#include <unordered_map>

/**
 * Object graph representation and utilities.
 * 
 * Manages object graph structure during deserialization.
 */

struct ObjectGraphNode {
    void* object;
    std::vector<size_t> references;
    size_t classId;
};

class ObjectGraph {
public:
    size_t addNode(std::unique_ptr<ObjectGraphNode> node);
    ObjectGraphNode* getNode(size_t index);
    void linkNodes(size_t from, size_t to);
    
private:
    std::vector<std::unique_ptr<ObjectGraphNode>> nodes;
};

#endif

