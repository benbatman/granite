#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "consistent_hash_ring.h"
#include "read_file.h"
#include "feature_store.grpc.pb.h"
#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>

class RouterServiceImpl final : public featurestore::FeatureStoreService::Service
{
public:
    // The constructor takes a list of all available shard addresses
    // & CA cert
    RouterServiceImpl(const std::vector<std::string> &shard_addresses,
                      const std::string &ca_cert_path)
    {
        // Read root CA cert
        std::string root_cert = readFileToString(ca_cert_path);

        // Create SSL/TLS cred object
        grpc::SslCredentialsOptions ssl_opts;
        ssl_opts.pem_root_certs = root_cert;
        // Acting as client here, so using SslCredentials, not SslServerCredentials
        auto channel_creds = grpc::SslCredentials(ssl_opts);

        for (const auto &address : shard_addresses)
        {
            // Add shard to the consistent hash ring
            ring_.AddNode(address);

            // Create a secure gRPC channel using credentials
            auto channel = grpc::CreateChannel(address, channel_creds);
            stubs_[address] = featurestore::FeatureStoreService::NewStub(channel);

            std::cout << "Router connected to shard at " << address << " using TLS" << std::endl;
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

void RunRouter(const std::string &ca_path, const std::string &key_path,
               const std::string &cert_path, const std::vector<std::string> &shard_addresses)
{
    std::string router_address("0.0.0.0:50050");
    RouterServiceImpl service(shard_addresses, ca_path);

    // Read private key and certificate
    std::string private_key = readFileToString(key_path);
    std::string certificate = readFileToString(cert_path);

    // Create SSL/TSL cred object
    grpc::SslServerCredentialsOptions ssl_opts;
    ssl_opts.pem_key_cert_pairs.push_back({private_key, certificate});
    auto server_creds = grpc::SslServerCredentials(ssl_opts);

    grpc::EnableDefaultHealthCheckService(true);
    grpc::reflection::InitProtoReflectionServerBuilderPlugin();

    grpc::ServerBuilder builder;
    builder.AddListeningPort(router_address, server_creds);
    builder.RegisterService(&service);

    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    std::cout << "Router listening on " << router_address << std::endl;

    server->Wait();
}

int main(int argc, char **argv)
{
    if (argc < 5)
    {
        std::cerr << "Usage: " << argv[0] << " <ca_cert_path> <server_key_path> <server_cert_path> <shard_address_1> ..." << std::endl;
        return 1;
    }
    std::vector<std::string> shards;
    for (int i = 4; i < argc; ++i)
    {
        shards.push_back(argv[i]);
    }
    RunRouter(argv[1], argv[2], argv[3], shards);
    return 0;
}
