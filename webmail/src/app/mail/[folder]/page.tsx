'use client';

import { useState, useEffect, useCallback } from 'react';
import { useParams } from 'next/navigation';
import { EmailList, EmailView } from '@/components';
import { Email } from '@/types';

export default function FolderPage() {
  const params = useParams();
  const folder = decodeURIComponent(params.folder as string);

  const [emails, setEmails] = useState<Email[]>([]);
  const [selectedEmail, setSelectedEmail] = useState<Email | null>(null);
  const [loading, setLoading] = useState(true);
  const [detailLoading, setDetailLoading] = useState(false);
  const [page, setPage] = useState(1);
  const [hasMore, setHasMore] = useState(false);
  const [total, setTotal] = useState(0);

  const fetchEmails = useCallback(async (pageNum: number = 1, append: boolean = false) => {
    try {
      if (!append) setLoading(true);

      const response = await fetch(
        `/api/emails?folder=${encodeURIComponent(folder)}&page=${pageNum}&pageSize=50`
      );
      const data = await response.json();

      if (data.success) {
        if (append) {
          setEmails((prev) => [...prev, ...data.data.items]);
        } else {
          setEmails(data.data.items);
        }
        setHasMore(data.data.hasMore);
        setTotal(data.data.total);
        setPage(pageNum);
      }
    } catch (error) {
      console.error('Failed to fetch emails:', error);
    } finally {
      setLoading(false);
    }
  }, [folder]);

  useEffect(() => {
    setSelectedEmail(null);
    fetchEmails(1);
  }, [folder, fetchEmails]);

  const handleSelectEmail = async (email: Email) => {
    if (selectedEmail?.uid === email.uid) return;

    setDetailLoading(true);
    try {
      const response = await fetch(
        `/api/emails/${email.uid}?folder=${encodeURIComponent(folder)}`
      );
      const data = await response.json();

      if (data.success) {
        setSelectedEmail(data.data);
        // Update seen status in list
        if (!email.seen) {
          setEmails((prev) =>
            prev.map((e) => (e.uid === email.uid ? { ...e, seen: true } : e))
          );
        }
      }
    } catch (error) {
      console.error('Failed to fetch email:', error);
    } finally {
      setDetailLoading(false);
    }
  };

  const handleToggleFlag = async (email: Email) => {
    try {
      await fetch(`/api/emails/${email.uid}?folder=${encodeURIComponent(folder)}`, {
        method: 'PATCH',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ flagged: !email.flagged }),
      });

      setEmails((prev) =>
        prev.map((e) =>
          e.uid === email.uid ? { ...e, flagged: !e.flagged } : e
        )
      );

      if (selectedEmail?.uid === email.uid) {
        setSelectedEmail((prev) => prev ? { ...prev, flagged: !prev.flagged } : null);
      }
    } catch (error) {
      console.error('Failed to toggle flag:', error);
    }
  };

  const handleDelete = async (email: Email) => {
    try {
      await fetch(`/api/emails/${email.uid}?folder=${encodeURIComponent(folder)}`, {
        method: 'DELETE',
      });

      setEmails((prev) => prev.filter((e) => e.uid !== email.uid));
      if (selectedEmail?.uid === email.uid) {
        setSelectedEmail(null);
      }
    } catch (error) {
      console.error('Failed to delete email:', error);
    }
  };

  const mailActions = typeof window !== 'undefined' ? (window as any).__mailActions : null;

  const handleReply = (email: Email) => {
    mailActions?.reply(email);
  };

  const handleReplyAll = (email: Email) => {
    mailActions?.replyAll(email);
  };

  const handleForward = (email: Email) => {
    mailActions?.forward(email);
  };

  const loadMore = () => {
    if (hasMore && !loading) {
      fetchEmails(page + 1, true);
    }
  };

  return (
    <div className="flex h-full">
      {/* Email List */}
      <div className="w-full md:w-96 border-r border-gray-200 flex flex-col">
        <div className="p-4 border-b border-gray-200">
          <h2 className="text-lg font-semibold text-gray-900">{folder}</h2>
          <p className="text-sm text-gray-500">{total} emails</p>
        </div>

        <div className="flex-1 overflow-y-auto">
          <EmailList
            emails={emails}
            selectedId={selectedEmail?.uid}
            onSelect={handleSelectEmail}
            onToggleFlag={handleToggleFlag}
            loading={loading}
          />

          {hasMore && (
            <div className="p-4 text-center">
              <button
                onClick={loadMore}
                className="text-primary-600 hover:text-primary-700 text-sm font-medium"
              >
                Load more emails...
              </button>
            </div>
          )}
        </div>
      </div>

      {/* Email View */}
      <div className="hidden md:flex flex-1 flex-col">
        <EmailView
          email={selectedEmail}
          onReply={handleReply}
          onReplyAll={handleReplyAll}
          onForward={handleForward}
          onDelete={handleDelete}
          onToggleFlag={handleToggleFlag}
          onClose={() => setSelectedEmail(null)}
          loading={detailLoading}
        />
      </div>
    </div>
  );
}
