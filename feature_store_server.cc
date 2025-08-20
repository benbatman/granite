// feature_store_server.cc

#include <iostream>
#include <memory>
#include <string>

// Core gRPC headers
#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>

// The C++ code generated from .proto file
#include "feature_store.grpc.pb.h"

// RockDB headers
#include "rocksdb/db.h"
#include "rocksdb/options.h"

// Helper function to create composite key
std::string makeFeatureKey(const std::string &entity_id, const std::string &feature_name)
{
    return entity_id + ":" + feature_name;
}

class FeatureStoreServiceImpl final : public featurestore::FeatureStoreService::Service
{
public:
    // Constructor: Open the RocksDB database when service is created
    FeatureStoreServiceImpl(const std::string &db_path)
    {
        rocksdb::Options options;
        // Create the database if it doesn't already exist
        options.create_if_missing = true;

        rocksdb::DB *db_ptr;
        rocksdb::Status status = rocksdb::DB::Open(options, db_path, &db_ptr);

        if (!status.ok())
        {
            std::cerr << "Failed to open RocksDB: " << status.ToString() << std::endl;
            exit(EXIT_FAILURE); // Will change
        }
        // Assign raw pointer to the unique ptr for automatic memory management
        db_.reset(db_ptr);
        std::cout << "RocksDB opened successfully at " << db_path << std::endl;
    }

private:
    // the `override` keyword ensures we are correctly implementing the virtual function from the base class
    grpc::Status PutFeatures(grpc::ServerContext *context,
                             const featurestore::PutFeaturesRequest *request,
                             featurestore::PutFeaturesResponse *response) override
    {

        std::cout << "Received PutFeatures for entity: " << request->entity_id() << std::endl;
        rocksdb::WriteOptions write_options;

        // -> is member access operator for pointer
        for (const auto &feature : request->features())
        {
            std::string key = makeFeatureKey(request->entity_id(), feature.feature_name());
            std::string value{};

            // Serialize the Feature protobuf message into a string (binary format).
            // Value contains serialized feature data
            if (!feature.SerializeToString(&value))
            {
                return grpc::Status(grpc::StatusCode::INTERNAL, "Failed to serialize feature.");
            }

            // Write the key-value pair to RocksDB.
            rocksdb::Status status = db_->Put(write_options, key, value);
            if (!status.ok())
            {
                std::string error_msg = "Failed to write to RocksDB: " + status.ToString();
                return grpc::Status(grpc::StatusCode::INTERNAL, error_msg);
            }
        }

        return grpc::Status::OK;
    }

    // GetFeatures
    grpc::Status GetFeatures(grpc::ServerContext *context,
                             const featurestore::GetFeaturesRequest *request,
                             featurestore::GetFeaturesResponse *response) override
    {

        std::cout << "Received GetFeatures for entity: " << request->entity_id() << std::endl;

        response->set_entity_id(request->entity_id());
        response->set_feature_group(request->feature_group());

        // Use an iterator to find all features for the given entity.
        std::unique_ptr<rocksdb::Iterator> it(db_->NewIterator(rocksdb::ReadOptions()));

        // The prefix we are scanning for.
        const std::string prefix = request->entity_id() + ":";

        for (it->Seek(prefix); it->Valid() && it->key().starts_with(prefix); it->Next())
        {
            featurestore::Feature feature;
            // Parse the binary string value from RocksDB back into a Feature protobuf message.
            if (!feature.ParseFromString(it->value().ToString()))
            {
                return grpc::Status(grpc::StatusCode::INTERNAL, "Failed to parse feature.");
            }

            // Add the deserialized feature to our response map.
            // The key in the map is the feature name.
            (*response->mutable_features())[feature.feature_name()] = feature;
        }

        return grpc::Status::OK;
    }

    // Ensure the Rocksdb instance is properly closed and memory is freed when the service is destroyed.
    std::unique_ptr<rocksdb::DB> db_;
};

void RunServer(const std::string &port, const std::string &db_path)
{
    std::string server_address("0.0.0.0:" + port);
    // Pass the path for the database to the service constructor.
    FeatureStoreServiceImpl service(db_path);

    grpc::EnableDefaultHealthCheckService(true);
    grpc::reflection::InitProtoReflectionServerBuilderPlugin();

    grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    std::cout << "Shard server listening on " << server_address << " using db at " << db_path << std::endl;

    server->Wait();
}

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        std::cerr << "Usage: " << argv[0] << " <port> <db_path>" << std::endl;
        return 1;
    }

    std::string port = argv[1];
    std::string db_path = argv[2];

    RunServer(port, db_path);
    return 0;
}
