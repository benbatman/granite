# Granite

## Why Granite?

It sounded cool, and I was staring at RocksDB website so...yeah

**A Distributed, High-Performance Feature Store in C++**
Granite is a proof-of-concept distributed feature store built from the ground up in modern C++. It is designed for low-latency feature retrieval and horizontal scalability. The system uses a sharded architecture with a [gRPC](https://grpc.io/) interface, persistent storage via RocksDB, and intelligent request routing with a consistent hashing ring.

## Architecture

Granite's architecture is composed of two primary components: a **Router** and a set of **Shards**. Clients interact exclusively with the router, which provides a single entry point to the entire distributed system.

1. **Client:** An application that needs feature data for ML model inference or training.

2. **Router:** A stateless gRPC server that acts as a smart proxy. It receives client requests, uses a consistent hashing algorithm on the `entity_id` to determine which shard owns the data, and forwards the request to the appropriate shard.

3. **Shards:** Each shard is a gRPC server with an embedded RocksDB instance for persistent storage. It is responsible for a subset of the total data. The system can be scaled horizontally by adding more shards.

## Core Features

**Low-Latency gRPC API:** All communication uses gRPC with Protocol Buffers, providing a strongly-typed, high-performance, and language-agnostic API.

**Horizontally Scalable:** The sharded architecture allows the system to scale by simply adding more server nodes, increasing both storage capacity and throughput.

**Efficient Request Routing:** A consistent hashing ring implementation ensures that requests are correctly and efficiently routed to the shard that owns the data.

**Minimal Rebalancing:** The use of consistent hashing means that adding or removing a shard from the cluster causes minimal data reshuffling, ensuring high availability.

**Persistent Storage:** Each shard uses an embedded [RocksDB](https://github.com/facebook/rocksdb) instance, a high-performance key-value store, to persist features on disk.

**Modern C++ and Bazel:** Built with modern C++, and compiled with the Bazel build system for reproducible and hermetic builds.

## Design Choices

The technology stack for Granite was chosen to prioritize performance, scalability, and reliability.

**gRPC & Protocol Buffers:** Chosen over REST/JSON for its superior performance due to binary serialization, its strict schema enforcement which prevents data errors, and its native support for streaming and bidirectional communication.

**RocksDB:** Selected as the storage engine because it is an embedded library, eliminating the network overhead of a separate database server. It is optimized for fast storage (SSD/NVMe) and provides the high-performance Put, Get, and Scan operations needed by a feature store.

**Consistent Hashing:** This algorithm is the cornerstone of Granite's scalability. Unlike simple modulo hashing, it ensures that when the number of shards changes, only a small, necessary fraction of keys need to be remapped, making scaling operations smooth and non-disruptive.

**Bazel Build System:** Chosen to manage the complex dependencies of gRPC, Protobuf, and RocksDB.

## Prerequisites

To build and run Granite, you will need:

[Bazel](https://bazel.build/) (version 8.0 or newer recommended)

A C++ compiler that supports C++17 (e.g., GCC 9+, Clang 10+)

[grpcurl](https://github.com/fullstorydev/grpcurl) for interacting with the service from the command line.

## Build Instructions

1. **Clone the repository:**

```bash
git clone https://github.com/your_username/granite.git
cd granite
```

2. **Build all targets (router, server/shard, and libraries):**

```bash
bazel build //...
```

## Usage

Granite is a distributed system and requires running multiple processes. Open a separate terminal for the router and for each shard you wish to run.

1. **Start the Shards**
   Launch each shard server, providing a unique port and database path for each.

- **Terminal 1 (Shard 1):**

```bash
./bazel-bin/feature_store_server 50051 /tmp/granite_db1
```

**Terminal 2 (Shard 2):**

```bash
./bazel-bin/feature_store_server 50052 /tmp/granite_db2
```

**Terminal 3 (Shard 3):**

```bash
./bazel-bin/feature_store_server 50053 /tmp/granite_db3
```

2. **Start the Router**
   In a new terminal, start the router and provide it with the list of running shard addresses.

- **Terminal 4 (Router):**

```bash
./bazel-bin/feature_store_router localhost:50051 localhost:50052 localhost:50053
```

The router will now be listening for client requests on port 50050.

3. **Interact with the Feature Store**
   Use `grpcurl` in a new terminal to send requests to the router.
   These commands are also in the `examples/` directory.

- **Write Features:**

```bash
grpcurl -plaintext -d '{
  "feature_group": "user_profile",
  "entity_id": "user_alpha",
  "features": [
    { "feature_name": "age", "value": { "int64_value": 35 } },
    { "feature_name": "city", "value": { "string_value": "Fairfax" } }
  ]
}' localhost:50050 featurestore.FeatureStoreService/PutFeatures
```

- **Read Features:**

- **Read Features:**

```bash
grpcurl -plaintext -d '{
  "feature_group": "user_profile",
  "entity_id": "user_alpha"
}' localhost:50050 featurestore.FeatureStoreService/GetFeatures
```

You can monitor the terminal output of the router and shards to see the requests being routed and processed.

## Roadmap

- **Security:** Implement TLS encryption for all gRPC communication.

- **Client Libraries:** Develop client libraries in other languages (e.g., Python) for easier integration.

- **Observability:** Add Prometheus metrics for monitoring request latency, error rates, and system load.

- **Fault Tolerance:** Implement retry logic in the router and replication between shards.

- **Advanced Features:** Support for feature versioning and time-to-live (TTL) on features.

License
This project is licensed under the APACHE License. See the LICENSE file for details.
