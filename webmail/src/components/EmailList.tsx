'use client';

import { Email } from '@/types';
import { cn } from '@/lib/utils';
import Avatar from './Avatar';

interface EmailListProps {
  emails: Email[];
  selectedId?: number;
  onSelect: (email: Email) => void;
  onToggleFlag: (email: Email) => void;
  loading?: boolean;
}

function formatDate(date: Date | string): string {
  const now = new Date();
  const emailDate = new Date(date);
  const diffDays = Math.floor((now.getTime() - emailDate.getTime()) / (1000 * 60 * 60 * 24));

  if (diffDays === 0) {
    return emailDate.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' });
  } else if (diffDays === 1) {
    return 'Yesterday';
  } else if (diffDays < 7) {
    return emailDate.toLocaleDateString([], { weekday: 'short' });
  } else {
    return emailDate.toLocaleDateString([], { month: 'short', day: 'numeric' });
  }
}

function truncate(str: string, length: number): string {
  if (str.length <= length) return str;
  return str.slice(0, length) + '...';
}

export default function EmailList({
  emails,
  selectedId,
  onSelect,
  onToggleFlag,
  loading,
}: EmailListProps) {
  if (loading) {
    return (
      <div className="flex items-center justify-center h-full">
        <div className="animate-spin rounded-full h-8 w-8 border-b-2 border-primary-600" />
      </div>
    );
  }

  if (emails.length === 0) {
    return (
      <div className="flex flex-col items-center justify-center h-full text-gray-500">
        <span className="text-4xl mb-2">📭</span>
        <p>No emails in this folder</p>
      </div>
    );
  }

  return (
    <div className="divide-y divide-gray-100">
      {emails.map((email) => (
        <div
          key={email.uid}
          className={cn(
            'flex items-start gap-3 p-4 cursor-pointer transition-colors',
            selectedId === email.uid ? 'bg-primary-50' : 'hover:bg-gray-50',
            !email.seen && 'bg-blue-50/50'
          )}
          onClick={() => onSelect(email)}
        >
          <Avatar
            name={email.from.name || email.from.address}
            email={email.from.address}
            size="md"
          />

          <div className="flex-1 min-w-0">
            <div className="flex items-center justify-between gap-2">
              <span
                className={cn(
                  'truncate',
                  !email.seen ? 'font-semibold text-gray-900' : 'text-gray-700'
                )}
              >
                {email.from.name || email.from.address}
              </span>
              <span className="text-xs text-gray-500 whitespace-nowrap">
                {formatDate(email.date)}
              </span>
            </div>

            <div className="flex items-center gap-2">
              <span
                className={cn(
                  'truncate',
                  !email.seen ? 'font-medium text-gray-900' : 'text-gray-600'
                )}
              >
                {email.subject || '(No subject)'}
              </span>
              {email.hasAttachments && (
                <span className="text-gray-400" title="Has attachments">
                  📎
                </span>
              )}
            </div>

            <p className="text-sm text-gray-500 truncate">
              {truncate(email.preview || '', 100)}
            </p>
          </div>

          <button
            onClick={(e) => {
              e.stopPropagation();
              onToggleFlag(email);
            }}
            className={cn(
              'p-1 rounded transition-colors',
              email.flagged
                ? 'text-yellow-500 hover:text-yellow-600'
                : 'text-gray-300 hover:text-gray-400'
            )}
            title={email.flagged ? 'Unflag' : 'Flag'}
          >
            ⭐
          </button>
        </div>
      ))}
    </div>
  );
}
