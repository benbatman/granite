#!/bin/bash

grpcurl -plaintext -d '{
  "feature_group": "user_profile",
  "entity_id": "user_alpha",
  "features": [
    {
      "feature_name": "age",
      "value": {
        "int64_value": 35
      }
    },
    {
      "feature_name": "city",
      "value": {
        "string_value": "Fairfax"
      }
    }
  ]
}' localhost:50050 featurestore.FeatureStoreService/PutFeatures
