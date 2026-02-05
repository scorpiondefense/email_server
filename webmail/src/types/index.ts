export interface User {
  email: string;
  name?: string;
}

export interface Email {
  id: string;
  uid: number;
  messageId: string;
  subject: string;
  from: EmailAddress;
  to: EmailAddress[];
  cc?: EmailAddress[];
  bcc?: EmailAddress[];
  replyTo?: EmailAddress[];
  date: Date | string;
  text?: string;
  html?: string;
  preview?: string;
  body?: {
    text?: string;
    html?: string;
  };
  attachments?: Attachment[];
  flags?: string[];
  folder?: string;
  seen: boolean;
  flagged: boolean;
  answered?: boolean;
  hasAttachments?: boolean;
}

export interface EmailAddress {
  name?: string;
  address: string;
}

export interface Attachment {
  filename: string;
  contentType: string;
  size: number;
  contentId?: string;
  content?: string; // Base64 encoded
}

export interface Folder {
  name: string;
  path: string;
  delimiter: string;
  flags: string[];
  specialUse?: string;
  total: number;
  unseen: number;
}

export interface EmailListItem {
  id: string;
  uid: number;
  subject: string;
  from: EmailAddress;
  date: Date | string;
  seen: boolean;
  flagged: boolean;
  answered?: boolean;
  hasAttachments?: boolean;
  preview?: string;
}

export interface ComposeEmail {
  to: string[];
  cc?: string[];
  bcc?: string[];
  subject: string;
  body: string;
  isHtml?: boolean;
  attachments?: File[];
  replyTo?: string;
  inReplyTo?: string;
}

export interface Session {
  user: User;
  accessToken: string;
  expiresAt: number;
}

export interface ApiResponse<T = unknown> {
  success: boolean;
  data?: T;
  error?: string;
}

export interface PaginatedResponse<T> {
  items: T[];
  total: number;
  page: number;
  pageSize: number;
  hasMore: boolean;
}
