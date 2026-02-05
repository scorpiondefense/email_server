#!/bin/bash
#
# Generate self-signed SSL certificates for the email server
#
# Usage: ./generate_certs.sh <domain> [output_directory]
#
# Example: ./generate_certs.sh mail.example.com /etc/ssl/email_server
#

set -e

DOMAIN="${1:-mail.example.com}"
OUTPUT_DIR="${2:-./certs}"
DAYS=365
KEY_SIZE=4096

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}Email Server Certificate Generator${NC}"
echo "======================================"
echo ""
echo "Domain: $DOMAIN"
echo "Output: $OUTPUT_DIR"
echo "Validity: $DAYS days"
echo "Key size: $KEY_SIZE bits"
echo ""

# Create output directory
mkdir -p "$OUTPUT_DIR"

# Generate CA key and certificate
echo -e "${YELLOW}Generating CA certificate...${NC}"
openssl genrsa -out "$OUTPUT_DIR/ca.key" $KEY_SIZE

openssl req -new -x509 -days $DAYS -key "$OUTPUT_DIR/ca.key" \
    -out "$OUTPUT_DIR/ca.crt" \
    -subj "/C=US/ST=State/L=City/O=Email Server/CN=Email Server CA"

# Generate server key
echo -e "${YELLOW}Generating server key...${NC}"
openssl genrsa -out "$OUTPUT_DIR/server.key" $KEY_SIZE

# Create OpenSSL config for SAN
cat > "$OUTPUT_DIR/openssl.cnf" << EOF
[req]
default_bits = $KEY_SIZE
prompt = no
default_md = sha256
distinguished_name = dn
req_extensions = req_ext

[dn]
C = US
ST = State
L = City
O = Email Server
CN = $DOMAIN

[req_ext]
subjectAltName = @alt_names
keyUsage = digitalSignature, keyEncipherment
extendedKeyUsage = serverAuth

[alt_names]
DNS.1 = $DOMAIN
DNS.2 = *.$DOMAIN
DNS.3 = localhost
IP.1 = 127.0.0.1
EOF

# Generate certificate signing request
echo -e "${YELLOW}Generating certificate signing request...${NC}"
openssl req -new -key "$OUTPUT_DIR/server.key" \
    -out "$OUTPUT_DIR/server.csr" \
    -config "$OUTPUT_DIR/openssl.cnf"

# Create extension file for certificate
cat > "$OUTPUT_DIR/cert_ext.cnf" << EOF
authorityKeyIdentifier=keyid,issuer
basicConstraints=CA:FALSE
keyUsage = digitalSignature, nonRepudiation, keyEncipherment, dataEncipherment
subjectAltName = @alt_names

[alt_names]
DNS.1 = $DOMAIN
DNS.2 = *.$DOMAIN
DNS.3 = localhost
IP.1 = 127.0.0.1
EOF

# Sign the certificate with our CA
echo -e "${YELLOW}Signing certificate...${NC}"
openssl x509 -req -days $DAYS \
    -in "$OUTPUT_DIR/server.csr" \
    -CA "$OUTPUT_DIR/ca.crt" \
    -CAkey "$OUTPUT_DIR/ca.key" \
    -CAcreateserial \
    -out "$OUTPUT_DIR/server.crt" \
    -extfile "$OUTPUT_DIR/cert_ext.cnf"

# Create combined certificate chain
cat "$OUTPUT_DIR/server.crt" "$OUTPUT_DIR/ca.crt" > "$OUTPUT_DIR/server-chain.crt"

# Clean up temporary files
rm -f "$OUTPUT_DIR/server.csr" "$OUTPUT_DIR/openssl.cnf" "$OUTPUT_DIR/cert_ext.cnf" "$OUTPUT_DIR/ca.srl"

# Set appropriate permissions
chmod 600 "$OUTPUT_DIR"/*.key
chmod 644 "$OUTPUT_DIR"/*.crt

# Verify the certificate
echo ""
echo -e "${YELLOW}Verifying certificate...${NC}"
openssl verify -CAfile "$OUTPUT_DIR/ca.crt" "$OUTPUT_DIR/server.crt"

echo ""
echo -e "${GREEN}Certificates generated successfully!${NC}"
echo ""
echo "Files created:"
echo "  CA Certificate:     $OUTPUT_DIR/ca.crt"
echo "  CA Private Key:     $OUTPUT_DIR/ca.key"
echo "  Server Certificate: $OUTPUT_DIR/server.crt"
echo "  Server Private Key: $OUTPUT_DIR/server.key"
echo "  Certificate Chain:  $OUTPUT_DIR/server-chain.crt"
echo ""
echo "Configuration for server.conf:"
echo ""
echo "[tls]"
echo "certificate = $OUTPUT_DIR/server.crt"
echo "private_key = $OUTPUT_DIR/server.key"
echo "ca_file = $OUTPUT_DIR/ca.crt"
echo ""
echo -e "${YELLOW}Note: For production, use certificates from Let's Encrypt or another CA.${NC}"
echo ""
echo "To use Let's Encrypt:"
echo "  1. Install certbot: apt install certbot"
echo "  2. Run: certbot certonly --standalone -d $DOMAIN"
echo "  3. Certificates will be in /etc/letsencrypt/live/$DOMAIN/"
echo ""
