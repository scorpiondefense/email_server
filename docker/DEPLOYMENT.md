# Cloud VPS Deployment Guide

This guide walks you through deploying the email server on a cloud VPS (DigitalOcean, Linode, Vultr, AWS, etc.).

## Prerequisites

- A VPS with Ubuntu 22.04 LTS (1GB RAM minimum, 2GB recommended)
- A domain name with DNS access
- Docker and Docker Compose installed
- Root or sudo access

## Step 1: VPS Setup

### Install Docker

```bash
# Update system
sudo apt update && sudo apt upgrade -y

# Install Docker
curl -fsSL https://get.docker.com | sh

# Add your user to docker group
sudo usermod -aG docker $USER

# Install Docker Compose
sudo apt install docker-compose-plugin -y

# Log out and back in for group changes
exit
```

### Configure Firewall

```bash
# Install UFW if not present
sudo apt install ufw -y

# Allow SSH (important - do this first!)
sudo ufw allow 22/tcp

# Allow email ports
sudo ufw allow 25/tcp    # SMTP
sudo ufw allow 465/tcp   # SMTPS
sudo ufw allow 587/tcp   # Submission
sudo ufw allow 110/tcp   # POP3
sudo ufw allow 995/tcp   # POP3S
sudo ufw allow 143/tcp   # IMAP
sudo ufw allow 993/tcp   # IMAPS

# Enable firewall
sudo ufw enable
```

## Step 2: DNS Configuration

Configure the following DNS records for your domain. Replace:
- `XXX.XXX.XXX.XXX` with your VPS IP address
- `example.com` with your domain

| Type | Name | Value | TTL |
|------|------|-------|-----|
| A | mail.example.com | XXX.XXX.XXX.XXX | 300 |
| MX | example.com | mail.example.com | 300 |
| TXT | example.com | v=spf1 mx ~all | 300 |

### Request PTR Record (Reverse DNS)

Contact your VPS provider to set up reverse DNS:
- **IP**: XXX.XXX.XXX.XXX
- **PTR**: mail.example.com

This is crucial for email deliverability!

## Step 3: Deploy Email Server

### Clone and Configure

```bash
# Clone the repository
git clone https://github.com/yourusername/email_server.git
cd email_server

# Create environment file
cp .env.example .env

# Edit configuration
nano .env
```

### Update .env File

```bash
# Your VPS public IP
SERVER_IP=XXX.XXX.XXX.XXX

# Your domain
MAIL_DOMAIN=example.com
MAIL_HOSTNAME=mail.example.com

# Admin credentials (change these!)
ADMIN_EMAIL=admin@example.com
ADMIN_PASSWORD=YourSecurePassword123!

# Enable relay for sending external mail
SMTP_ALLOW_RELAY=true
```

### Build and Start

```bash
# Build the Docker image
docker-compose build

# Start the services
docker-compose up -d

# Check logs
docker-compose logs -f
```

## Step 4: SSL Certificates

### Option A: Self-Signed (for testing)

Self-signed certificates are generated automatically on first run.

### Option B: Let's Encrypt (for production)

```bash
# Stop the email server temporarily
docker-compose down

# Install certbot
sudo apt install certbot -y

# Get certificates (make sure ports 80/443 are open temporarily)
sudo certbot certonly --standalone -d mail.example.com

# Copy certificates to the Docker volume
sudo cp /etc/letsencrypt/live/mail.example.com/fullchain.pem ./certs/server.crt
sudo cp /etc/letsencrypt/live/mail.example.com/privkey.pem ./certs/server.key
sudo cp /etc/letsencrypt/live/mail.example.com/chain.pem ./certs/ca.crt

# Update docker-compose.yml to mount the certs directory
# Or copy to the Docker volume location

# Restart
docker-compose up -d
```

### Auto-Renewal for Let's Encrypt

```bash
# Create renewal script
sudo nano /etc/cron.d/certbot-email

# Add this line:
0 0 1 * * root certbot renew --quiet && docker-compose -f /path/to/email_server/docker-compose.yml restart
```

## Step 5: User Management

### Create Users

```bash
# Create a new user
docker-compose exec email create-user add user@example.com

# List users
docker-compose exec email create-user list

# Change password
docker-compose exec email create-user passwd user@example.com

# Delete user
docker-compose exec email create-user delete user@example.com
```

### Add Domains

```bash
# Add another domain
docker-compose exec email create-user domain add anotherdomain.com

# List domains
docker-compose exec email create-user domain list
```

## Step 6: Verify Installation

### Test SMTP

```bash
# Test with openssl
openssl s_client -connect mail.example.com:465

# Or with telnet (plain)
telnet mail.example.com 25
EHLO test
QUIT
```

### Test IMAP

```bash
openssl s_client -connect mail.example.com:993
```

### Test POP3

```bash
openssl s_client -connect mail.example.com:995
```

### Send Test Email

```bash
# Install swaks for testing
sudo apt install swaks -y

# Send test email
swaks --to user@example.com \
      --from admin@example.com \
      --server mail.example.com:587 \
      --auth LOGIN \
      --auth-user admin@example.com \
      --auth-password YourPassword \
      --tls
```

## Step 7: Configure Email Clients

### Thunderbird / Outlook / Apple Mail

**Incoming Mail (IMAP - Recommended):**
- Server: mail.example.com
- Port: 993
- Security: SSL/TLS
- Username: user@example.com

**Incoming Mail (POP3):**
- Server: mail.example.com
- Port: 995
- Security: SSL/TLS
- Username: user@example.com

**Outgoing Mail (SMTP):**
- Server: mail.example.com
- Port: 587
- Security: STARTTLS
- Authentication: Normal password
- Username: user@example.com

## Maintenance

### View Logs

```bash
# All logs
docker-compose logs -f

# SMTP only
docker-compose logs -f | grep SMTP

# Last 100 lines
docker-compose logs --tail=100
```

### Backup

```bash
# Backup mail data
docker run --rm -v email_server_mail_data:/data -v $(pwd):/backup \
    ubuntu tar cvzf /backup/mail_backup_$(date +%Y%m%d).tar.gz /data

# Backup database
docker run --rm -v email_server_db_data:/data -v $(pwd):/backup \
    ubuntu tar cvzf /backup/db_backup_$(date +%Y%m%d).tar.gz /data
```

### Restore

```bash
# Restore mail data
docker run --rm -v email_server_mail_data:/data -v $(pwd):/backup \
    ubuntu tar xvzf /backup/mail_backup_YYYYMMDD.tar.gz -C /

# Restore database
docker run --rm -v email_server_db_data:/data -v $(pwd):/backup \
    ubuntu tar xvzf /backup/db_backup_YYYYMMDD.tar.gz -C /
```

### Update

```bash
# Pull latest changes
git pull

# Rebuild and restart
docker-compose build
docker-compose up -d
```

## Troubleshooting

### Connection Refused

```bash
# Check if containers are running
docker-compose ps

# Check if ports are listening
ss -tlnp | grep -E '(25|110|143|465|587|993|995)'

# Check firewall
sudo ufw status
```

### Authentication Failed

```bash
# Check user exists
docker-compose exec email create-user info user@example.com

# Reset password
docker-compose exec email create-user passwd user@example.com
```

### TLS Errors

```bash
# Verify certificate
openssl s_client -connect mail.example.com:993 -showcerts

# Check certificate dates
echo | openssl s_client -connect mail.example.com:993 2>/dev/null | openssl x509 -dates
```

### Mail Not Delivered

```bash
# Check logs for errors
docker-compose logs | grep -i error

# Verify DNS
dig MX example.com
dig A mail.example.com

# Check reverse DNS
dig -x XXX.XXX.XXX.XXX
```

### Email Marked as Spam

1. Verify PTR record matches mail hostname
2. Add SPF record: `v=spf1 mx ~all`
3. Consider adding DKIM (requires additional setup)
4. Add DMARC record: `v=DMARC1; p=none; rua=mailto:admin@example.com`

## Security Recommendations

1. **Use strong passwords** for all email accounts
2. **Enable fail2ban** to prevent brute force attacks
3. **Keep system updated**: `apt update && apt upgrade`
4. **Monitor logs** for suspicious activity
5. **Use Let's Encrypt** for production certificates
6. **Regular backups** of mail and database
7. **Disable root login** via SSH

## Quick Reference

```bash
# Start
docker-compose up -d

# Stop
docker-compose down

# Restart
docker-compose restart

# Logs
docker-compose logs -f

# Shell access
docker-compose exec email bash

# Create user
docker-compose exec email create-user add user@example.com
```
