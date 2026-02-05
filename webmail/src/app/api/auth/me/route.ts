import { NextResponse } from 'next/server';
import { getCredentials } from '@/lib/auth';

export async function GET() {
  try {
    const credentials = await getCredentials();

    if (!credentials) {
      return NextResponse.json(
        { success: false, error: 'Not authenticated' },
        { status: 401 }
      );
    }

    return NextResponse.json({
      success: true,
      data: {
        email: credentials.email,
      },
    });
  } catch (error) {
    console.error('Get user error:', error);
    return NextResponse.json(
      { success: false, error: 'Failed to get user info' },
      { status: 500 }
    );
  }
}
