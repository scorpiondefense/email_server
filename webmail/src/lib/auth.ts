import { SignJWT, jwtVerify } from 'jose';
import { cookies } from 'next/headers';
import type { Session, User } from '@/types';

const JWT_SECRET = new TextEncoder().encode(
  process.env.JWT_SECRET || 'default-secret-change-in-production'
);
const SESSION_EXPIRY = parseInt(process.env.SESSION_EXPIRY || '86400', 10);
const COOKIE_NAME = 'webmail_session';

export interface SessionPayload {
  user: User;
  email: string;
  password: string; // Encrypted or we need it for IMAP/SMTP
  expiresAt: number;
}

export async function createSession(
  email: string,
  password: string
): Promise<string> {
  const expiresAt = Date.now() + SESSION_EXPIRY * 1000;

  const user: User = {
    email,
    name: email.split('@')[0],
  };

  const token = await new SignJWT({
    user,
    email,
    password: Buffer.from(password).toString('base64'),
    expiresAt,
  })
    .setProtectedHeader({ alg: 'HS256' })
    .setIssuedAt()
    .setExpirationTime(expiresAt / 1000)
    .sign(JWT_SECRET);

  return token;
}

export async function verifySession(token: string): Promise<SessionPayload | null> {
  try {
    const { payload } = await jwtVerify(token, JWT_SECRET);

    if (typeof payload.expiresAt === 'number' && payload.expiresAt < Date.now()) {
      return null;
    }

    return payload as unknown as SessionPayload;
  } catch {
    return null;
  }
}

export async function getSession(): Promise<SessionPayload | null> {
  const cookieStore = cookies();
  const token = cookieStore.get(COOKIE_NAME)?.value;

  if (!token) {
    return null;
  }

  return verifySession(token);
}

export async function getCredentials(): Promise<{ email: string; password: string } | null> {
  const session = await getSession();

  if (!session) {
    return null;
  }

  return {
    email: session.email,
    password: Buffer.from(session.password, 'base64').toString('utf-8'),
  };
}

export function setSessionCookie(token: string) {
  cookies().set(COOKIE_NAME, token, {
    httpOnly: true,
    secure: process.env.NODE_ENV === 'production',
    sameSite: 'lax',
    maxAge: SESSION_EXPIRY,
    path: '/',
  });
}

export function clearSessionCookie() {
  cookies().delete(COOKIE_NAME);
}
