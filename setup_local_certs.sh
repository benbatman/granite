#!/bin/bash

# Create the CA private key
openssl genrsa -aes256 -out ca.key 2048

# Create the CA certificate
openssl req -x509 -new -nodes -key ca.key -sha256 -days 365 -out ca.crt

# Server private key
openssl genrsa -out server.key 2048

# Server CSR with SANs
openssl req -new -key server.key -out server.csr \
  -subj "/CN=localhost" \
  -addext "subjectAltName=DNS:localhost,IP:127.0.0.1,IP:::1"

# Sign CSR with CA
cat > server.ext << 'EOF'
basicConstraints=CA:FALSE
keyUsage = digitalSignature, keyEncipherment
extendedKeyUsage = serverAuth
subjectAltName = DNS:localhost,IP:127.0.0.1,IP:::1
EOF

# Create the server certificate
openssl x509 -req -in server.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out server.crt -days 365 -sha256 -extfile server.ext
