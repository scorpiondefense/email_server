import { NextRequest, NextResponse } from 'next/server';
import { createSession, setSessionCookie } from '@/lib/auth';
import { createImapConnection, connectImap, disconnectImap } from '@/lib/imap';

export async function POST(request: NextRequest) {
  try {
    const { email, password } = await request.json();

    if (!email || !password) {
      return NextResponse.json(
        { success: false, error: 'Email and password are required' },
        { status: 400 }
      );
    }

    // Verify credentials by attempting IMAP connection
    const imap = createImapConnection(email, password);

    try {
      await connectImap(imap);
      disconnectImap(imap);
    } catch (error) {
      console.error('Authentication failed:', error);
      return NextResponse.json(
        { success: false, error: 'Invalid email or password' },
        { status: 401 }
      );
    }

    // Create session token
    const token = await createSession(email, password);

    // Set cookie
    const response = NextResponse.json({
      success: true,
      data: {
        user: {
          email,
          name: email.split('@')[0],
        },
      },
    });

    response.cookies.set('webmail_session', token, {
      httpOnly: true,
      secure: process.env.NODE_ENV === 'production',
      sameSite: 'lax',
      maxAge: parseInt(process.env.SESSION_EXPIRY || '86400', 10),
      path: '/',
    });

    return response;
  } catch (error) {
    console.error('Login error:', error);
    return NextResponse.json(
      { success: false, error: 'An error occurred during login' },
      { status: 500 }
    );
  }
}
