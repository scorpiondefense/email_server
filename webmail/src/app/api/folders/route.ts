import { NextResponse } from 'next/server';
import { getCredentials } from '@/lib/auth';
import { getFolders } from '@/lib/imap';

export async function GET() {
  try {
    const credentials = await getCredentials();

    if (!credentials) {
      return NextResponse.json(
        { success: false, error: 'Not authenticated' },
        { status: 401 }
      );
    }

    const folders = await getFolders(credentials.email, credentials.password);

    // Sort folders with special folders first
    const sortOrder = ['inbox', 'sent', 'drafts', 'junk', 'trash', 'archive'];
    folders.sort((a, b) => {
      const aIndex = sortOrder.indexOf(a.specialUse || '');
      const bIndex = sortOrder.indexOf(b.specialUse || '');

      if (aIndex !== -1 && bIndex !== -1) return aIndex - bIndex;
      if (aIndex !== -1) return -1;
      if (bIndex !== -1) return 1;
      return a.name.localeCompare(b.name);
    });

    return NextResponse.json({
      success: true,
      data: folders,
    });
  } catch (error) {
    console.error('Get folders error:', error);
    return NextResponse.json(
      { success: false, error: 'Failed to fetch folders' },
      { status: 500 }
    );
  }
}
