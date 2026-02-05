import { NextRequest, NextResponse } from 'next/server';
import { getCredentials } from '@/lib/auth';
import { getEmails } from '@/lib/imap';

export async function GET(request: NextRequest) {
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
    const page = parseInt(searchParams.get('page') || '1', 10);
    const pageSize = parseInt(searchParams.get('pageSize') || '50', 10);

    const { emails, total } = await getEmails(
      credentials.email,
      credentials.password,
      folder,
      page,
      pageSize
    );

    return NextResponse.json({
      success: true,
      data: {
        items: emails,
        total,
        page,
        pageSize,
        hasMore: page * pageSize < total,
      },
    });
  } catch (error) {
    console.error('Get emails error:', error);
    return NextResponse.json(
      { success: false, error: 'Failed to fetch emails' },
      { status: 500 }
    );
  }
}
