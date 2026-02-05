# Webmail Client

A modern Next.js webmail client for the email server suite.

## Features

- IMAP email access
- SMTP email sending
- Folder management (inbox, sent, drafts, trash, etc.)
- Email composition with attachments
- Reply, Reply All, Forward
- Flag/unflag emails
- Mark as read/unread
- Delete emails
- Responsive design

## Development

### Prerequisites

- Node.js 20+
- npm or yarn
- Running email server (POP3/IMAP/SMTP)

### Setup

1. Install dependencies:
   ```bash
   cd webmail
   npm install
   ```

2. Copy environment file:
   ```bash
   cp .env.local.example .env.local
   ```

3. Update `.env.local` with your email server settings:
   ```env
   IMAP_HOST=localhost
   IMAP_PORT=993
   IMAP_SECURE=true
   SMTP_HOST=localhost
   SMTP_PORT=465
   SMTP_SECURE=true
   JWT_SECRET=your-secret-key
   ```

4. Start the development server:
   ```bash
   npm run dev
   ```

5. Open [http://localhost:3000](http://localhost:3000) in your browser.

## Production Build

```bash
npm run build
npm start
```

## Docker

The webmail is included in the main docker-compose.yml. To run it:

```bash
# From the project root
docker-compose up -d
```

The webmail will be available at `http://YOUR_SERVER_IP:3000`.

## Environment Variables

| Variable | Description | Default |
|----------|-------------|---------|
| `JWT_SECRET` | Secret key for JWT session encryption | `change-this-in-production` |
| `SESSION_EXPIRY` | Session expiry in seconds | `86400` (24 hours) |
| `IMAP_HOST` | IMAP server hostname | `localhost` |
| `IMAP_PORT` | IMAP server port | `993` |
| `IMAP_SECURE` | Use TLS for IMAP | `true` |
| `SMTP_HOST` | SMTP server hostname | `localhost` |
| `SMTP_PORT` | SMTP server port | `465` |
| `SMTP_SECURE` | Use TLS for SMTP | `true` |

## Architecture

```
webmail/
├── src/
│   ├── app/                  # Next.js App Router
│   │   ├── (auth)/           # Auth routes (login)
│   │   ├── mail/             # Mail routes
│   │   └── api/              # API routes
│   ├── components/           # React components
│   ├── lib/                  # Utility libraries
│   │   ├── auth.ts           # JWT session management
│   │   ├── imap.ts           # IMAP client
│   │   ├── smtp.ts           # SMTP client
│   │   └── utils.ts          # Helper functions
│   └── types/                # TypeScript types
├── public/                   # Static assets
├── Dockerfile                # Docker build
└── package.json
```

## API Routes

| Method | Route | Description |
|--------|-------|-------------|
| POST | `/api/auth/login` | Authenticate user |
| POST | `/api/auth/logout` | End session |
| GET | `/api/auth/me` | Get current user |
| GET | `/api/folders` | List email folders |
| GET | `/api/emails` | List emails in folder |
| GET | `/api/emails/[uid]` | Get single email |
| PATCH | `/api/emails/[uid]` | Update email flags |
| DELETE | `/api/emails/[uid]` | Delete email |
| POST | `/api/send` | Send email |

## License

MIT
