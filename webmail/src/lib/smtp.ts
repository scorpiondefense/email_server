import nodemailer from 'nodemailer';
import type { ComposeEmail } from '@/types';
import { generateMessageId } from './utils';

const SMTP_HOST = process.env.SMTP_HOST || process.env.MAIL_SERVER_HOST || 'localhost';
const SMTP_PORT = parseInt(process.env.SMTP_PORT || '465', 10);
const SMTP_SECURE = process.env.SMTP_SECURE !== 'false';

export function createSmtpTransport(email: string, password: string) {
  return nodemailer.createTransport({
    host: SMTP_HOST,
    port: SMTP_PORT,
    secure: SMTP_SECURE,
    auth: {
      user: email,
      pass: password,
    },
    tls: {
      rejectUnauthorized: false, // Allow self-signed certs
    },
  });
}

export async function sendEmail(
  senderEmail: string,
  senderPassword: string,
  compose: ComposeEmail
): Promise<{ messageId: string }> {
  const transporter = createSmtpTransport(senderEmail, senderPassword);

  const domain = senderEmail.split('@')[1] || 'localhost';
  const messageId = generateMessageId(domain);

  const mailOptions: nodemailer.SendMailOptions = {
    from: senderEmail,
    to: compose.to.join(', '),
    cc: compose.cc?.join(', '),
    bcc: compose.bcc?.join(', '),
    subject: compose.subject,
    messageId,
    inReplyTo: compose.inReplyTo,
    references: compose.inReplyTo,
  };

  if (compose.isHtml) {
    mailOptions.html = compose.body;
    // Also include plain text version
    mailOptions.text = compose.body.replace(/<[^>]*>/g, '');
  } else {
    mailOptions.text = compose.body;
  }

  // Handle attachments if any
  if (compose.attachments && compose.attachments.length > 0) {
    mailOptions.attachments = await Promise.all(
      compose.attachments.map(async (file) => {
        const buffer = await file.arrayBuffer();
        return {
          filename: file.name,
          content: Buffer.from(buffer),
          contentType: file.type,
        };
      })
    );
  }

  const info = await transporter.sendMail(mailOptions);

  return { messageId: info.messageId };
}

export async function verifySmtpConnection(
  email: string,
  password: string
): Promise<boolean> {
  const transporter = createSmtpTransport(email, password);

  try {
    await transporter.verify();
    return true;
  } catch {
    return false;
  }
}
