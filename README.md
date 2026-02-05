# Email Server Suite

A complete private email server implementation in C++20 with POP3, IMAP, and SMTP support.

## Features

- **POP3 Server** (RFC 1939)
  - Ports: 110 (plain), 995 (TLS)
  - Commands: USER, PASS, STAT, LIST, RETR, DELE, NOOP, RSET, QUIT, TOP, UIDL, CAPA, STLS
  - STARTTLS support

- **IMAP Server** (RFC 3501)
  - Ports: 143 (plain), 993 (TLS)
  - Commands: LOGIN, LOGOUT, SELECT, EXAMINE, CREATE, DELETE, RENAME, LIST, STATUS, FETCH, STORE, COPY, SEARCH, EXPUNGE, UID, CAPABILITY, NOOP, STARTTLS
  - Message flags and mailbox management

- **SMTP Server** (RFC 5321)
  - Ports: 25 (MTA), 587 (submission), 465 (TLS)
  - Commands: HELO, EHLO, MAIL FROM, RCPT TO, DATA, RSET, NOOP, QUIT, VRFY, AUTH, STARTTLS
  - AUTH PLAIN and AUTH LOGIN
  - Local delivery and relay support

- **Common Features**
  - TLS/SSL encryption (STARTTLS and implicit TLS)
  - SQLite-based user authentication
  - Maildir storage format
  - Configurable via INI-style config file
  - Comprehensive logging

## Requirements

- C++20 compatible compiler (GCC 10+, Clang 12+)
- CMake 3.20+
- Boost 1.74+ (Asio)
- OpenSSL 1.1.1+
- SQLite 3.35+
- Catch2 3.0+ (for tests, automatically fetched if not found)

### Installing Dependencies

**Ubuntu/Debian:**
```bash
sudo apt update
sudo apt install build-essential cmake libboost-all-dev libssl-dev libsqlite3-dev
```

**macOS (Homebrew):**
```bash
brew install cmake boost openssl sqlite3
```

**Fedora:**
```bash
sudo dnf install gcc-c++ cmake boost-devel openssl-devel sqlite-devel
```

## Building

```bash
# Clone the repository
git clone https://github.com/yourusername/email_server.git
cd email_server

# Create build directory
mkdir build && cd build

# Configure
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build . -j$(nproc)

# Run tests
ctest --output-on-failure

# Install (optional)
sudo cmake --install .
```

### Build Options

- `-DBUILD_TESTS=ON|OFF` - Build test programs (default: ON)
- `-DBUILD_TOOLS=ON|OFF` - Build utility tools (default: ON)
- `-DENABLE_TLS=ON|OFF` - Enable TLS/SSL support (default: ON)
- `-DCMAKE_BUILD_TYPE=Debug|Release` - Build type

## Configuration

1. Copy the example configuration:
```bash
sudo mkdir -p /etc/email_server
sudo cp config/server.conf.example /etc/email_server/server.conf
```

2. Edit the configuration file:
```bash
sudo nano /etc/email_server/server.conf
```

Key settings to configure:
- `[tls]` - SSL certificate and key paths
- `[database]` - User database location
- `[storage]` - Maildir root directory
- `[smtp]` - SMTP server settings including hostname and local domains

## SSL Certificates

### Self-Signed (for testing)
```bash
./tools/generate_certs.sh mail.example.com /etc/ssl/email_server
```

### Let's Encrypt (for production)
```bash
# Install certbot
sudo apt install certbot

# Obtain certificate
sudo certbot certonly --standalone -d mail.example.com

# Update configuration
# certificate = /etc/letsencrypt/live/mail.example.com/fullchain.pem
# private_key = /etc/letsencrypt/live/mail.example.com/privkey.pem
```

## User Management

Use the `create_user` tool to manage users:

```bash
# Add a domain
./bin/create_user domain add example.com

# Add a user
./bin/create_user add user@example.com

# List users
./bin/create_user list

# Change password
./bin/create_user passwd user@example.com

# Show user info
./bin/create_user info user@example.com

# Delete user
./bin/create_user delete user@example.com
```

## Running the Servers

### Individual Servers

```bash
# SMTP server
./bin/smtp_server -c /etc/email_server/server.conf

# POP3 server
./bin/pop3_server -c /etc/email_server/server.conf

# IMAP server
./bin/imap_server -c /etc/email_server/server.conf
```

### Systemd Services

Create systemd service files for production deployment:

`/etc/systemd/system/email-smtp.service`:
```ini
[Unit]
Description=Email SMTP Server
After=network.target

[Service]
Type=simple
ExecStart=/usr/local/bin/smtp_server -c /etc/email_server/server.conf
Restart=always
User=mail
Group=mail

[Install]
WantedBy=multi-user.target
```

Enable and start:
```bash
sudo systemctl enable email-smtp email-pop3 email-imap
sudo systemctl start email-smtp email-pop3 email-imap
```

## DNS Configuration

Configure the following DNS records for your domain:

| Type | Name | Value | Priority |
|------|------|-------|----------|
| A | mail.example.com | YOUR_IP | - |
| MX | example.com | mail.example.com | 10 |
| TXT | example.com | v=spf1 mx -all | - |
| PTR | YOUR_IP | mail.example.com | - |

For SPF (prevents email spoofing):
```
v=spf1 mx -all
```

For DMARC (optional but recommended):
```
_dmarc.example.com TXT "v=DMARC1; p=quarantine; rua=mailto:admin@example.com"
```

## Firewall Configuration

Open the required ports:

```bash
# UFW
sudo ufw allow 25/tcp    # SMTP
sudo ufw allow 465/tcp   # SMTPS
sudo ufw allow 587/tcp   # Submission
sudo ufw allow 110/tcp   # POP3
sudo ufw allow 995/tcp   # POP3S
sudo ufw allow 143/tcp   # IMAP
sudo ufw allow 993/tcp   # IMAPS

# iptables
sudo iptables -A INPUT -p tcp --dport 25 -j ACCEPT
sudo iptables -A INPUT -p tcp --dport 465 -j ACCEPT
sudo iptables -A INPUT -p tcp --dport 587 -j ACCEPT
sudo iptables -A INPUT -p tcp --dport 110 -j ACCEPT
sudo iptables -A INPUT -p tcp --dport 995 -j ACCEPT
sudo iptables -A INPUT -p tcp --dport 143 -j ACCEPT
sudo iptables -A INPUT -p tcp --dport 993 -j ACCEPT
```

## Client Configuration

### Thunderbird

**Incoming Mail (IMAP):**
- Server: mail.example.com
- Port: 993
- Security: SSL/TLS
- Authentication: Normal password

**Incoming Mail (POP3):**
- Server: mail.example.com
- Port: 995
- Security: SSL/TLS
- Authentication: Normal password

**Outgoing Mail (SMTP):**
- Server: mail.example.com
- Port: 587
- Security: STARTTLS
- Authentication: Normal password

### Apple Mail

1. Add Account > Other Mail Account
2. Enter your name, email, and password
3. Configure manually:
   - IMAP: mail.example.com:993 (SSL)
   - SMTP: mail.example.com:587 (STARTTLS)

### Outlook

1. File > Add Account
2. Manual setup > POP or IMAP
3. Configure:
   - Account type: IMAP
   - Incoming: mail.example.com:993 (SSL)
   - Outgoing: mail.example.com:587 (TLS)

## Testing

### Manual Testing with telnet

**SMTP:**
```bash
telnet mail.example.com 25
EHLO client.example.com
AUTH PLAIN <base64-credentials>
MAIL FROM:<sender@example.com>
RCPT TO:<recipient@example.com>
DATA
Subject: Test
This is a test.
.
QUIT
```

**POP3:**
```bash
telnet mail.example.com 110
USER user@example.com
PASS password
STAT
LIST
RETR 1
QUIT
```

### Testing with OpenSSL (TLS)

```bash
openssl s_client -connect mail.example.com:993
openssl s_client -connect mail.example.com:995
openssl s_client -starttls smtp -connect mail.example.com:587
```

## Directory Structure

```
/var/mail/                    # Maildir root
  example.com/                # Domain
    username/                 # User maildir
      cur/                    # Read messages
      new/                    # New messages
      tmp/                    # Temporary
      .Sent/                  # Sent folder
      .Drafts/                # Drafts folder
      .Trash/                 # Trash folder

/var/lib/email_server/        # Server data
  users.db                    # User database

/etc/email_server/            # Configuration
  server.conf                 # Main config file

/var/log/email_server/        # Logs
  smtp.log
  pop3.log
  imap.log
```

## Troubleshooting

### Connection refused
- Check if the server is running: `systemctl status email-smtp`
- Check firewall rules: `sudo ufw status`
- Verify ports are listening: `ss -tlnp | grep -E '(25|110|143|465|587|993|995)'`

### Authentication failed
- Verify user exists: `./bin/create_user info user@example.com`
- Check password: `./bin/create_user passwd user@example.com`
- Check logs: `tail -f /var/log/email_server/smtp.log`

### TLS handshake failed
- Verify certificate: `openssl x509 -in /etc/ssl/email_server/server.crt -text`
- Check certificate chain: `openssl verify -CAfile ca.crt server.crt`
- Ensure key matches certificate

### Messages not delivered
- Check maildir permissions: `ls -la /var/mail/example.com/username/`
- Verify local_domains in config
- Check SMTP logs for delivery errors

## Security Recommendations

1. **Always use TLS** - Enable STARTTLS on all ports
2. **Require authentication** - Set `require_auth = true` for SMTP
3. **Use strong passwords** - Enforce via user management
4. **Regular updates** - Keep the system and dependencies updated
5. **Firewall** - Only open necessary ports
6. **SPF/DKIM/DMARC** - Configure email authentication
7. **Monitoring** - Set up log monitoring for suspicious activity
8. **Backups** - Regularly backup /var/mail and user database

## License

MIT License - See LICENSE file for details.

## Contributing

Contributions are welcome! Please submit pull requests or open issues on GitHub.
