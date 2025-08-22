#!/bin/bash

grpcurl -cacert ca.crt -d '{
  "feature_group": "user_profile",
  "entity_id": "user_alpha"
}' localhost:50050 featurestore.FeatureStoreService/GetFeatures
