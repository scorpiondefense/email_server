# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

```bash
# Build (Release)
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)

# Build (Debug with sanitizers)
cmake .. -DCMAKE_BUILD_TYPE=Debug
cmake --build . -j$(nproc)

# Run all tests
ctest --output-on-failure

# Run single test by name
ctest -R <test_name> --output-on-failure
```

**CMake Options:**
- `-DBUILD_TESTS=ON|OFF` (default: ON)
- `-DBUILD_TOOLS=ON|OFF` (default: ON)
- `-DENABLE_TLS=ON|OFF` (default: ON)

**Output:** Binaries in `build/bin/`, libraries in `build/lib/`

## Running Servers

```bash
./bin/smtp_server -c /etc/email_server/server.conf
./bin/pop3_server -c /etc/email_server/server.conf
./bin/imap_server -c /etc/email_server/server.conf
```

## Architecture Overview

This is a complete email server suite with SMTP, POP3, IMAP servers (C++20) and a Next.js webmail client.

### Core Components

**common/** - Shared static library (`email_common`) used by all servers:
- `config.hpp` - Singleton INI config parser
- `logger.hpp` - Thread-safe logging (Trace→Fatal levels) with rotation
- `ssl_context.hpp` - TLS certificate/key management
- `auth/authenticator.hpp` - SQLite user/domain DB with SHA256+salt hashing
- `storage/maildir.hpp` - Maildir format storage (RFC 3501), UID management
- `net/session.hpp` - Base async session class (Boost.Asio)
- `net/server.hpp` - Template `Server<SessionType>` for generic async TCP servers

**Protocol Servers** - Each builds as separate executable:
- `smtp/` - SMTP server (ports 25, 465, 587) - RFC 5321
- `pop3/` - POP3 server (ports 110, 995) - RFC 1939
- `imap/` - IMAP server (ports 143, 993) - RFC 3501

**webmail/** - Next.js web client (port 3000) with IMAP/SMTP client libs

### Key Patterns

**Async I/O**: All networking uses Boost.Asio with callback-based handlers and configurable thread pools.

**Server Template**: `Server<SessionType>` is a generic async TCP server. Protocol servers (SMTP, POP3, IMAP) inherit from `Session` and implement protocol-specific handlers.

**TLS Support**: Conditional compilation with `#ifdef ENABLE_TLS`. Sessions can upgrade from plain→TLS (STARTTLS) or use implicit TLS ports.

**Thread Safety**: Mutex protection on session sets, authenticator (SQLite), logger, and write queues.

### Storage

Maildir format at `/var/mail/<domain>/<user>/`:
- `cur/` - Read messages
- `new/` - Unread messages
- `tmp/` - Temporary storage
- `.Sent/`, `.Drafts/`, `.Trash/` - IMAP folders

Message flags encoded in filename: S=seen, R=replied, F=flagged, T=trashed, D=draft

### Database

SQLite (`users.db`) with tables:
- `users` - id, username, domain, password_hash, quota_bytes, used_bytes, active, created_at
- `domains` - id, domain, active

### Configuration

INI-style config at `/etc/email_server/server.conf` with sections: `[tls]`, `[database]`, `[storage]`, `[log]`, `[smtp]`, `[pop3]`, `[imap]`

See `config/server.conf.example` for all options.

## Docker

```bash
docker-compose build
docker-compose up -d
docker-compose exec email create-user add user@example.com
```

## Tools

```bash
# User management
./bin/create_user add user@example.com
./bin/create_user list
./bin/create_user domain add example.com
./bin/create_user passwd user@example.com

# Generate certificates
./tools/generate_certs.sh mail.example.com /etc/ssl/email_server
```

## Dependencies

- Boost 1.74+ (Asio)
- OpenSSL 1.1.1+
- SQLite3 3.35+
- Catch2 3.0+ (auto-fetched)
