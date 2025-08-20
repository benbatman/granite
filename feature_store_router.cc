#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "consistent_hash_ring.h"
#include "feature_store.grpc.pb.h"
#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>

class RouterServiceImpl final : public featurestore::FeatureStoreService::Service
{
public:
    // The constructor takes a list of all available shard addresses
    RouterServiceImpl(const std::vector<std::string> &shard_addresses)
    {
        for (const auto &address : shard_addresses)
        {
            // Add shard to the consistent hash ring
            ring_.AddNode(address);

            // Create a gRPC client "stub" for this shard
            auto channel = grpc::CreateChannel(address, grpc::InsecureChannelCredentials());
            stubs_[address] = featurestore::FeatureStoreService::NewStub(channel);

            std::cout << "Router connected to shard at " << address << std::endl;
        }
    }

    // Override the PutFeatures method to route the request
    grpc::Status PutFeatures(grpc::ServerContext *context,
                             const featurestore::PutFeaturesRequest *request,
                             featurestore::PutFeaturesResponse *response) override
    {

        // Get the entity_id from the incoming request
        const std::string &entity_id = request->entity_id();

        // Use the consistent hash ring to find the correct shard address
        std::string shard_address = ring_.GetNode(entity_id);
        if (shard_address.empty())
        {
            return grpc::Status(grpc::StatusCode::UNAVAILABLE, "No shard found for entity_id: " + entity_id);
        }

        std::cout << "Routing PutFeatures for entity: " << entity_id << " to shard: " << shard_address << std::endl;

        // Find the client stub for that shard
        auto &stub = stubs_[shard_address];

        // Act as a client: forward the request to the shard and get the response
        grpc::ClientContext client_context{};
        grpc::Status status = stub->PutFeatures(&client_context, *request, response);

        // Return the shard's response and status to the original client
        return status;
    }

    // The GetFeatures logic is identical, just calling a different stub method
    grpc::Status GetFeatures(grpc::ServerContext *context,
                             const featurestore::GetFeaturesRequest *request,
                             featurestore::GetFeaturesResponse *response) override
    {

        const std::string &entity_id = request->entity_id();
        std::string shard_address = ring_.GetNode(entity_id);
        if (shard_address.empty())
        {
            return grpc::Status(grpc::StatusCode::UNAVAILABLE, "No shard found for entity_id: " + entity_id);
        }

        std::cout << "Routing GetFeatures for entity " << entity_id << " to shard " << shard_address << std::endl;

        auto &stub = stubs_[shard_address];
        grpc::ClientContext client_context{};
        grpc::Status status = stub->GetFeatures(&client_context, *request, response);

        return status;
    }

private:
    ConsistentHashRing ring_;

    // A map from shard address to the client stub that can talk to it
    std::map<std::string, std::unique_ptr<featurestore::FeatureStoreService::Stub>> stubs_;
};

void RunRouter(const std::vector<std::string> &shard_addresses)
{
    std::string router_address("0.0.0.0:50050");
    RouterServiceImpl service(shard_addresses);

    grpc::EnableDefaultHealthCheckService(true);
    grpc::reflection::InitProtoReflectionServerBuilderPlugin();

    grpc::ServerBuilder builder;
    builder.AddListeningPort(router_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    std::cout << "Router listening on " << router_address << std::endl;

    server->Wait();
}

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0] << " <shard_address_1> <shard_address_2> ..." << std::endl;
        return 1;
    }
    std::vector<std::string> shards;
    for (int i = 1; i < argc; ++i)
    {
        shards.push_back(argv[i]);
    }
    RunRouter(shards);
    return 0;
}
