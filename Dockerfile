# Email Server Dockerfile
# Multi-stage build for minimal production image

# Build stage
FROM ubuntu:22.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    libboost-all-dev \
    libssl-dev \
    libsqlite3-dev \
    git \
    && rm -rf /var/lib/apt/lists/*

# Copy source code
WORKDIR /src
COPY . .

# Build the project
RUN mkdir build && cd build && \
    cmake .. \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_TESTS=OFF \
        -DENABLE_TLS=ON && \
    cmake --build . -j$(nproc)

# Runtime stage
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

# Install runtime dependencies
RUN apt-get update && apt-get install -y \
    libboost-system1.74.0 \
    libssl3 \
    libsqlite3-0 \
    openssl \
    dnsutils \
    && rm -rf /var/lib/apt/lists/*

# Create mail user and directories
RUN useradd -r -s /bin/false mail && \
    mkdir -p /var/mail \
             /var/lib/email_server \
             /var/log/email_server \
             /etc/email_server \
             /etc/ssl/email_server && \
    chown -R mail:mail /var/mail \
                       /var/lib/email_server \
                       /var/log/email_server

# Copy binaries from builder
COPY --from=builder /src/build/bin/smtp_server /usr/local/bin/
COPY --from=builder /src/build/bin/pop3_server /usr/local/bin/
COPY --from=builder /src/build/bin/imap_server /usr/local/bin/
COPY --from=builder /src/build/bin/create_user /usr/local/bin/

# Copy configuration
COPY config/server.conf.example /etc/email_server/server.conf.example
COPY tools/generate_certs.sh /usr/local/bin/
RUN chmod +x /usr/local/bin/generate_certs.sh

# Copy entrypoint script
COPY docker/entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh

# Expose ports
# SMTP
EXPOSE 25 465 587
# POP3
EXPOSE 110 995
# IMAP
EXPOSE 143 993

# Volumes for persistent data
VOLUME ["/var/mail", "/var/lib/email_server", "/etc/ssl/email_server", "/var/log/email_server"]

ENTRYPOINT ["/entrypoint.sh"]
CMD ["all"]
