import { NextRequest, NextResponse } from 'next/server';
import { getCredentials } from '@/lib/auth';
import { getEmail, deleteEmail, markAsRead, toggleFlag, moveEmail } from '@/lib/imap';

export async function GET(
  request: NextRequest,
  { params }: { params: { uid: string } }
) {
  try {
    const credentials = await getCredentials();

    if (!credentials) {
      return NextResponse.json(
        { success: false, error: 'Not authenticated' },
        { status: 401 }
      );
    }

    const searchParams = request.nextUrl.searchParams;
    const folder = searchParams.get('folder') || 'INBOX';
    const uid = parseInt(params.uid, 10);

    const email = await getEmail(
      credentials.email,
      credentials.password,
      folder,
      uid
    );

    if (!email) {
      return NextResponse.json(
        { success: false, error: 'Email not found' },
        { status: 404 }
      );
    }

    // Mark as read
    if (!email.seen) {
      await markAsRead(credentials.email, credentials.password, folder, uid);
      email.seen = true;
    }

    return NextResponse.json({
      success: true,
      data: email,
    });
  } catch (error) {
    console.error('Get email error:', error);
    return NextResponse.json(
      { success: false, error: 'Failed to fetch email' },
      { status: 500 }
    );
  }
}

export async function DELETE(
  request: NextRequest,
  { params }: { params: { uid: string } }
) {
  try {
    const credentials = await getCredentials();

    if (!credentials) {
      return NextResponse.json(
        { success: false, error: 'Not authenticated' },
        { status: 401 }
      );
    }

    const searchParams = request.nextUrl.searchParams;
    const folder = searchParams.get('folder') || 'INBOX';
    const uid = parseInt(params.uid, 10);

    await deleteEmail(credentials.email, credentials.password, folder, uid);

    return NextResponse.json({ success: true });
  } catch (error) {
    console.error('Delete email error:', error);
    return NextResponse.json(
      { success: false, error: 'Failed to delete email' },
      { status: 500 }
    );
  }
}

export async function PATCH(
  request: NextRequest,
  { params }: { params: { uid: string } }
) {
  try {
    const credentials = await getCredentials();

    if (!credentials) {
      return NextResponse.json(
        { success: false, error: 'Not authenticated' },
        { status: 401 }
      );
    }

    const searchParams = request.nextUrl.searchParams;
    const folder = searchParams.get('folder') || 'INBOX';
    const uid = parseInt(params.uid, 10);

    const body = await request.json();

    // Handle flag changes
    if (body.flagged !== undefined) {
      await toggleFlag(
        credentials.email,
        credentials.password,
        folder,
        uid,
        '\\Flagged',
        body.flagged
      );
    }

    if (body.seen !== undefined) {
      await toggleFlag(
        credentials.email,
        credentials.password,
        folder,
        uid,
        '\\Seen',
        body.seen
      );
    }

    // Handle move
    if (body.moveTo) {
      await moveEmail(
        credentials.email,
        credentials.password,
        folder,
        body.moveTo,
        uid
      );
    }

    return NextResponse.json({ success: true });
  } catch (error) {
    console.error('Update email error:', error);
    return NextResponse.json(
      { success: false, error: 'Failed to update email' },
      { status: 500 }
    );
  }
}
