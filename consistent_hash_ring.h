#ifndef CONSISTENT_HASH_RING_H
#define CONSISTENT_HASH_RING_H

#include <map>
#include <string>
#include <vector>

class ConsistentHashRing
{
public:
    // Adds a node (a shard server address) to the hash ring
    void AddNode(const std::string &node);

    // Removes a node from the hash ring
    void RemoveNode(const std::string &node);

    // Gets the node responsible for the given key
    std::string GetNode(const std::string &key) const;

private:
    // The ring itself is a map where the key is the hash value (position on the ring)
    // and the value is the node identifier (server address).
    // std::map automatically keeps keys sorted
    std::map<size_t, std::string> ring_;
};

#endif // CONSISTENT_HASH_RING_H
