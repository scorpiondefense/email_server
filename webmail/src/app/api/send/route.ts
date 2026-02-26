import { NextRequest, NextResponse } from 'next/server';
import { getCredentials } from '@/lib/auth';
import { sendEmail } from '@/lib/smtp';

export async function POST(request: NextRequest) {
  try {
    const credentials = await getCredentials();

    if (!credentials) {
      return NextResponse.json(
        { success: false, error: 'Not authenticated' },
        { status: 401 }
      );
    }

    const formData = await request.formData();

    const to = formData.get('to') as string;
    const cc = formData.get('cc') as string;
    const bcc = formData.get('bcc') as string;
    const subject = formData.get('subject') as string;
    const body = formData.get('body') as string;
    const isHtml = formData.get('isHtml') === 'true';
    const inReplyTo = formData.get('inReplyTo') as string;

    if (!to || !subject) {
      return NextResponse.json(
        { success: false, error: 'To and subject are required' },
        { status: 400 }
      );
    }

    // Parse comma-separated email addresses
    const toAddresses = to.split(',').map((e) => e.trim()).filter(Boolean);
    const ccAddresses = cc ? cc.split(',').map((e) => e.trim()).filter(Boolean) : undefined;
    const bccAddresses = bcc ? bcc.split(',').map((e) => e.trim()).filter(Boolean) : undefined;

    // Handle attachments
    const attachments: File[] = [];
    for (const [key, value] of Array.from(formData.entries())) {
      if (key.startsWith('attachment') && value instanceof File) {
        attachments.push(value);
      }
    }

    const result = await sendEmail(credentials.email, credentials.password, {
      to: toAddresses,
      cc: ccAddresses,
      bcc: bccAddresses,
      subject,
      body: body || '',
      isHtml,
      attachments: attachments.length > 0 ? attachments : undefined,
      inReplyTo: inReplyTo || undefined,
    });

    return NextResponse.json({
      success: true,
      data: { messageId: result.messageId },
    });
  } catch (error) {
    console.error('Send email error:', error);
    return NextResponse.json(
      { success: false, error: 'Failed to send email' },
      { status: 500 }
    );
  }
}
