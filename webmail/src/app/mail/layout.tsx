'use client';

import { useState, useEffect } from 'react';
import { useRouter } from 'next/navigation';
import { Sidebar, ComposeModal } from '@/components';
import { Folder, Email } from '@/types';

export default function MailLayout({
  children,
}: {
  children: React.ReactNode;
}) {
  const router = useRouter();
  const [folders, setFolders] = useState<Folder[]>([]);
  const [userEmail, setUserEmail] = useState('');
  const [composeOpen, setComposeOpen] = useState(false);
  const [composeProps, setComposeProps] = useState<{
    to?: string;
    cc?: string;
    subject?: string;
    body?: string;
    inReplyTo?: string;
  }>({});

  useEffect(() => {
    fetchFolders();
    fetchUserInfo();
  }, []);

  const fetchFolders = async () => {
    try {
      const response = await fetch('/api/folders');
      const data = await response.json();

      if (data.success) {
        setFolders(data.data);
      }
    } catch (error) {
      console.error('Failed to fetch folders:', error);
    }
  };

  const fetchUserInfo = async () => {
    try {
      const response = await fetch('/api/auth/me');
      const data = await response.json();

      if (data.success) {
        setUserEmail(data.data.email);
      }
    } catch (error) {
      console.error('Failed to fetch user info:', error);
    }
  };

  const handleLogout = async () => {
    try {
      await fetch('/api/auth/logout', { method: 'POST' });
      router.push('/login');
    } catch (error) {
      console.error('Logout error:', error);
    }
  };

  const handleCompose = () => {
    setComposeProps({});
    setComposeOpen(true);
  };

  const handleReply = (email: Email) => {
    const replyTo = email.replyTo?.[0] || email.from;
    setComposeProps({
      to: replyTo.address,
      subject: email.subject?.startsWith('Re:') ? email.subject : `Re: ${email.subject}`,
      body: `\n\n---\nOn ${new Date(email.date).toLocaleString()}, ${email.from.name || email.from.address} wrote:\n\n${email.text || ''}`,
      inReplyTo: email.messageId,
    });
    setComposeOpen(true);
  };

  const handleReplyAll = (email: Email) => {
    const replyTo = email.replyTo?.[0] || email.from;
    const allRecipients = [
      replyTo.address,
      ...email.to.map((a) => a.address),
    ].filter((addr, index, self) => self.indexOf(addr) === index && addr !== userEmail);

    const ccRecipients = email.cc?.map((a) => a.address).filter((addr) => addr !== userEmail) || [];

    setComposeProps({
      to: allRecipients.join(', '),
      cc: ccRecipients.join(', '),
      subject: email.subject?.startsWith('Re:') ? email.subject : `Re: ${email.subject}`,
      body: `\n\n---\nOn ${new Date(email.date).toLocaleString()}, ${email.from.name || email.from.address} wrote:\n\n${email.text || ''}`,
      inReplyTo: email.messageId,
    });
    setComposeOpen(true);
  };

  const handleForward = (email: Email) => {
    const attachmentInfo = email.attachments?.length
      ? `\n[${email.attachments.length} attachment(s)]`
      : '';

    setComposeProps({
      subject: email.subject?.startsWith('Fwd:') ? email.subject : `Fwd: ${email.subject}`,
      body: `\n\n---\nForwarded message:\nFrom: ${email.from.name || email.from.address}\nDate: ${new Date(email.date).toLocaleString()}\nSubject: ${email.subject}\nTo: ${email.to.map((a) => a.name || a.address).join(', ')}\n${attachmentInfo}\n\n${email.text || ''}`,
    });
    setComposeOpen(true);
  };

  // Expose compose functions globally for child components
  useEffect(() => {
    (window as any).__mailActions = {
      compose: handleCompose,
      reply: handleReply,
      replyAll: handleReplyAll,
      forward: handleForward,
    };
  }, [userEmail]);

  return (
    <div className="flex h-screen bg-white">
      <Sidebar
        folders={folders}
        onCompose={handleCompose}
        onLogout={handleLogout}
        userEmail={userEmail}
      />

      <main className="flex-1 flex flex-col overflow-hidden">
        {children}
      </main>

      <ComposeModal
        isOpen={composeOpen}
        onClose={() => setComposeOpen(false)}
        initialTo={composeProps.to}
        initialCc={composeProps.cc}
        initialSubject={composeProps.subject}
        initialBody={composeProps.body}
        inReplyTo={composeProps.inReplyTo}
      />
    </div>
  );
}
