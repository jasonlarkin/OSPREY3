#include "object_graph.h"

size_t ObjectGraph::addNode(std::unique_ptr<ObjectGraphNode> node) {
    size_t index = nodes.size();
    nodes.push_back(std::move(node));
    return index;
}

ObjectGraphNode* ObjectGraph::getNode(size_t index) {
    if (index >= nodes.size()) {
        return nullptr;
    }
    return nodes[index].get();
}

void ObjectGraph::linkNodes(size_t from, size_t to) {
    if (from >= nodes.size() || to >= nodes.size()) {
        return;
    }
    nodes[from]->references.push_back(to);
}

