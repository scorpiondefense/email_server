'use client';

import { useState } from 'react';
import { Email } from '@/types';
import { cn } from '@/lib/utils';
import Avatar from './Avatar';
import Button from './Button';

interface EmailViewProps {
  email: Email | null;
  onReply: (email: Email) => void;
  onReplyAll: (email: Email) => void;
  onForward: (email: Email) => void;
  onDelete: (email: Email) => void;
  onToggleFlag: (email: Email) => void;
  onClose: () => void;
  loading?: boolean;
}

function formatFullDate(date: Date | string): string {
  return new Date(date).toLocaleDateString([], {
    weekday: 'long',
    year: 'numeric',
    month: 'long',
    day: 'numeric',
    hour: '2-digit',
    minute: '2-digit',
  });
}

function formatAddresses(addresses: { name?: string; address: string }[]): string {
  return addresses
    .map((a) => (a.name ? `${a.name} <${a.address}>` : a.address))
    .join(', ');
}

export default function EmailView({
  email,
  onReply,
  onReplyAll,
  onForward,
  onDelete,
  onToggleFlag,
  onClose,
  loading,
}: EmailViewProps) {
  const [showDetails, setShowDetails] = useState(false);

  if (loading) {
    return (
      <div className="flex items-center justify-center h-full">
        <div className="animate-spin rounded-full h-8 w-8 border-b-2 border-primary-600" />
      </div>
    );
  }

  if (!email) {
    return (
      <div className="flex flex-col items-center justify-center h-full text-gray-500">
        <span className="text-4xl mb-2">📧</span>
        <p>Select an email to read</p>
      </div>
    );
  }

  return (
    <div className="flex flex-col h-full">
      {/* Header */}
      <div className="flex items-center justify-between p-4 border-b border-gray-200">
        <button
          onClick={onClose}
          className="p-2 rounded-lg hover:bg-gray-100 transition-colors md:hidden"
        >
          ← Back
        </button>

        <div className="flex items-center gap-2">
          <Button variant="ghost" size="sm" onClick={() => onReply(email)}>
            ↩️ Reply
          </Button>
          <Button variant="ghost" size="sm" onClick={() => onReplyAll(email)}>
            ↩️↩️ Reply All
          </Button>
          <Button variant="ghost" size="sm" onClick={() => onForward(email)}>
            ➡️ Forward
          </Button>
          <Button
            variant="ghost"
            size="sm"
            onClick={() => onToggleFlag(email)}
            className={email.flagged ? 'text-yellow-500' : ''}
          >
            ⭐ {email.flagged ? 'Unflag' : 'Flag'}
          </Button>
          <Button variant="danger" size="sm" onClick={() => onDelete(email)}>
            🗑️ Delete
          </Button>
        </div>
      </div>

      {/* Email Content */}
      <div className="flex-1 overflow-y-auto">
        <div className="p-6">
          {/* Subject */}
          <h1 className="text-2xl font-semibold text-gray-900 mb-4">
            {email.subject || '(No subject)'}
          </h1>

          {/* Sender */}
          <div className="flex items-start gap-4 mb-4">
            <Avatar
              name={email.from.name || email.from.address}
              email={email.from.address}
              size="lg"
            />

            <div className="flex-1">
              <div className="flex items-center justify-between">
                <div>
                  <span className="font-medium text-gray-900">
                    {email.from.name || email.from.address}
                  </span>
                  {email.from.name && (
                    <span className="text-gray-500 text-sm ml-2">
                      &lt;{email.from.address}&gt;
                    </span>
                  )}
                </div>
                <span className="text-sm text-gray-500">
                  {formatFullDate(email.date)}
                </span>
              </div>

              <button
                onClick={() => setShowDetails(!showDetails)}
                className="text-sm text-gray-500 hover:text-gray-700"
              >
                To: {email.to.map((a) => a.name || a.address).join(', ')}
                {showDetails ? ' ▲' : ' ▼'}
              </button>

              {showDetails && (
                <div className="mt-2 text-sm text-gray-500 space-y-1">
                  <p>
                    <strong>From:</strong> {formatAddresses([email.from])}
                  </p>
                  <p>
                    <strong>To:</strong> {formatAddresses(email.to)}
                  </p>
                  {email.cc && email.cc.length > 0 && (
                    <p>
                      <strong>Cc:</strong> {formatAddresses(email.cc)}
                    </p>
                  )}
                  {email.replyTo && email.replyTo.length > 0 && (
                    <p>
                      <strong>Reply-To:</strong> {formatAddresses(email.replyTo)}
                    </p>
                  )}
                </div>
              )}
            </div>
          </div>

          {/* Attachments */}
          {email.attachments && email.attachments.length > 0 && (
            <div className="mb-4 p-3 bg-gray-50 rounded-lg">
              <p className="text-sm font-medium text-gray-700 mb-2">
                📎 Attachments ({email.attachments.length})
              </p>
              <div className="flex flex-wrap gap-2">
                {email.attachments.map((attachment, index) => (
                  <a
                    key={index}
                    href={`/api/emails/${email.uid}/attachments/${index}`}
                    download={attachment.filename}
                    className="inline-flex items-center gap-1 px-3 py-1 bg-white border border-gray-200 rounded-lg text-sm text-gray-700 hover:bg-gray-100 transition-colors"
                  >
                    📄 {attachment.filename}
                    <span className="text-gray-400 text-xs">
                      ({Math.round((attachment.size || 0) / 1024)}KB)
                    </span>
                  </a>
                ))}
              </div>
            </div>
          )}

          {/* Body */}
          <div className="border-t border-gray-200 pt-4">
            {email.html ? (
              <iframe
                srcDoc={email.html}
                className="w-full min-h-[400px] border-0"
                sandbox="allow-same-origin"
                title="Email content"
              />
            ) : (
              <pre className="whitespace-pre-wrap font-sans text-gray-800">
                {email.text}
              </pre>
            )}
          </div>
        </div>
      </div>
    </div>
  );
}
