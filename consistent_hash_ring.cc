#include "consistent_hash_ring.h"
#include <functional> // for std::hash

void ConsistentHashRing::AddNode(const std::string &node)
{
    size_t hash = std::hash<std::string>{}(node);
    ring_[hash] = node;
}

void ConsistentHashRing::RemoveNode(const std::string &node)
{
    size_t hash = std::hash<std::string>{}(node);
    ring_.erase(hash);
}

std::string ConsistentHashRing::GetNode(const std::string &key) const
{
    if (ring_.empty())
    {
        return "";
    }

    size_t hash = std::hash<std::string>{}(key);

    auto it = ring_.lower_bound(hash);

    if (it == ring_.end())
    {
        // If lower_bound returns the end, it means the key's hash is greater
        // than any node's hash, need to wrap around the ring
        // The owner is the very first node in the map
        return ring_.begin()->second;
    }

    // Otherwise, the owner is the node pointed to by the iterator
    return it->second;
}
