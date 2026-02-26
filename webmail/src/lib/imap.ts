import Imap from 'imap';
import { simpleParser, ParsedMail } from 'mailparser';
import type { Email, EmailListItem, Folder, EmailAddress, Attachment } from '@/types';

const IMAP_HOST = process.env.IMAP_HOST || process.env.MAIL_SERVER_HOST || 'localhost';
const IMAP_PORT = parseInt(process.env.IMAP_PORT || '993', 10);
const IMAP_SECURE = process.env.IMAP_SECURE !== 'false';

export function createImapConnection(email: string, password: string): Imap {
  return new Imap({
    user: email,
    password: password,
    host: IMAP_HOST,
    port: IMAP_PORT,
    tls: IMAP_SECURE,
    tlsOptions: {
      rejectUnauthorized: false, // Allow self-signed certs
    },
    authTimeout: 10000,
    connTimeout: 30000,
  });
}

export function connectImap(imap: Imap): Promise<void> {
  return new Promise((resolve, reject) => {
    imap.once('ready', () => resolve());
    imap.once('error', (err: Error) => reject(err));
    imap.connect();
  });
}

export function disconnectImap(imap: Imap): void {
  try {
    imap.end();
  } catch {
    // Ignore errors on disconnect
  }
}

export async function getFolders(email: string, password: string): Promise<Folder[]> {
  const imap = createImapConnection(email, password);

  try {
    await connectImap(imap);

    return new Promise((resolve, reject) => {
      imap.getBoxes((err, boxes) => {
        if (err) {
          reject(err);
          return;
        }

        const folders: Folder[] = [];

        function processBoxes(boxes: Imap.MailBoxes, prefix = '') {
          for (const [name, box] of Object.entries(boxes)) {
            const path = prefix ? `${prefix}${box.delimiter}${name}` : name;

            // Map common folder names to special use
            let specialUse: string | undefined;
            const lowerName = name.toLowerCase();
            if (lowerName === 'inbox') specialUse = 'inbox';
            else if (lowerName === 'sent' || lowerName === 'sent mail') specialUse = 'sent';
            else if (lowerName === 'drafts') specialUse = 'drafts';
            else if (lowerName === 'trash' || lowerName === 'deleted') specialUse = 'trash';
            else if (lowerName === 'junk' || lowerName === 'spam') specialUse = 'junk';
            else if (lowerName === 'archive') specialUse = 'archive';

            folders.push({
              name,
              path,
              delimiter: box.delimiter,
              flags: box.attribs || [],
              specialUse,
              total: 0,
              unseen: 0,
            });

            if (box.children) {
              processBoxes(box.children, path);
            }
          }
        }

        processBoxes(boxes);

        // Get message counts for each folder
        const folderPromises = folders.map(async (folder) => {
          return new Promise<Folder>((resolveFolder) => {
            imap.openBox(folder.path, true, (err, box) => {
              if (!err && box) {
                folder.total = box.messages.total;
                folder.unseen = box.messages.unseen;
              }
              resolveFolder(folder);
            });
          });
        });

        Promise.all(folderPromises).then((updatedFolders) => {
          disconnectImap(imap);
          resolve(updatedFolders);
        });
      });
    });
  } catch (error) {
    disconnectImap(imap);
    throw error;
  }
}

export async function getEmails(
  email: string,
  password: string,
  folder: string = 'INBOX',
  page: number = 1,
  pageSize: number = 50
): Promise<{ emails: EmailListItem[]; total: number }> {
  const imap = createImapConnection(email, password);

  try {
    await connectImap(imap);

    return new Promise((resolve, reject) => {
      imap.openBox(folder, true, (err, box) => {
        if (err) {
          reject(err);
          return;
        }

        const total = box.messages.total;

        if (total === 0) {
          disconnectImap(imap);
          resolve({ emails: [], total: 0 });
          return;
        }

        // Calculate range (newest first)
        const start = Math.max(1, total - (page * pageSize) + 1);
        const end = Math.max(1, total - ((page - 1) * pageSize));

        if (start > total) {
          disconnectImap(imap);
          resolve({ emails: [], total });
          return;
        }

        const fetch = imap.seq.fetch(`${start}:${end}`, {
          bodies: ['HEADER.FIELDS (FROM TO SUBJECT DATE MESSAGE-ID)'],
          struct: true,
        });

        const emails: EmailListItem[] = [];

        fetch.on('message', (msg, seqno) => {
          const emailData: Partial<EmailListItem> = {
            id: '',
            uid: 0,
            subject: '',
            from: { address: '' },
            date: '',
            seen: false,
            flagged: false,
            answered: false,
            hasAttachments: false,
          };

          msg.on('body', (stream) => {
            let buffer = '';
            stream.on('data', (chunk) => {
              buffer += chunk.toString('utf8');
            });
            stream.once('end', () => {
              const headers = Imap.parseHeader(buffer);
              emailData.subject = headers.subject?.[0] || '(No Subject)';
              emailData.date = headers.date?.[0] || new Date().toISOString();
              emailData.id = headers['message-id']?.[0] || `${seqno}`;

              if (headers.from?.[0]) {
                const fromMatch = headers.from[0].match(/(?:"?([^"]*)"?\s)?<?([^>]+)>?/);
                if (fromMatch) {
                  emailData.from = {
                    name: fromMatch[1]?.trim(),
                    address: fromMatch[2]?.trim() || headers.from[0],
                  };
                } else {
                  emailData.from = { address: headers.from[0] };
                }
              }
            });
          });

          msg.once('attributes', (attrs) => {
            emailData.uid = attrs.uid;
            emailData.seen = attrs.flags.includes('\\Seen');
            emailData.flagged = attrs.flags.includes('\\Flagged');
            emailData.answered = attrs.flags.includes('\\Answered');

            // Check for attachments in structure
            if (attrs.struct) {
              emailData.hasAttachments = hasAttachments(attrs.struct);
            }
          });

          msg.once('end', () => {
            emails.push(emailData as EmailListItem);
          });
        });

        fetch.once('error', (err) => {
          reject(err);
        });

        fetch.once('end', () => {
          disconnectImap(imap);
          // Sort by date descending (newest first)
          emails.sort((a, b) => new Date(b.date).getTime() - new Date(a.date).getTime());
          resolve({ emails, total });
        });
      });
    });
  } catch (error) {
    disconnectImap(imap);
    throw error;
  }
}

function hasAttachments(struct: unknown[]): boolean {
  for (const part of struct) {
    if (Array.isArray(part)) {
      if (hasAttachments(part)) return true;
    } else if (typeof part === 'object' && part !== null) {
      const p = part as Record<string, unknown>;
      if (p.disposition && (p.disposition as Record<string, unknown>).type === 'attachment') {
        return true;
      }
      if (p.subtype === 'mixed' || p.subtype === 'alternative') {
        continue;
      }
    }
  }
  return false;
}

export async function getEmail(
  email: string,
  password: string,
  folder: string,
  uid: number
): Promise<Email | null> {
  const imap = createImapConnection(email, password);

  try {
    await connectImap(imap);

    return new Promise((resolve, reject) => {
      imap.openBox(folder, true, (err) => {
        if (err) {
          reject(err);
          return;
        }

        const fetch = imap.fetch(uid, {
          bodies: '',
          struct: true,
        });

        let emailData: Email | null = null;

        fetch.on('message', (msg) => {
          msg.on('body', (stream) => {
            simpleParser(stream as unknown as import('stream').Readable, (parseErr, parsed) => {
              if (parseErr) {
                console.error('Parse error:', parseErr);
                return;
              }

              emailData = parsedMailToEmail(parsed, folder, uid);
            });
          });

          msg.once('attributes', (attrs) => {
            if (emailData) {
              emailData.flags = attrs.flags;
              emailData.seen = attrs.flags.includes('\\Seen');
              emailData.flagged = attrs.flags.includes('\\Flagged');
              emailData.answered = attrs.flags.includes('\\Answered');
            }
          });
        });

        fetch.once('error', (err) => {
          reject(err);
        });

        fetch.once('end', () => {
          disconnectImap(imap);
          resolve(emailData);
        });
      });
    });
  } catch (error) {
    disconnectImap(imap);
    throw error;
  }
}

function parsedMailToEmail(parsed: ParsedMail, folder: string, uid: number): Email {
  const fromAddress: EmailAddress = parsed.from?.value[0]
    ? { name: parsed.from.value[0].name, address: parsed.from.value[0].address || '' }
    : { address: '' };

  const toAddresses: EmailAddress[] = parsed.to
    ? (Array.isArray(parsed.to) ? parsed.to : [parsed.to]).flatMap((t) =>
        t.value.map((addr) => ({ name: addr.name, address: addr.address || '' }))
      )
    : [];

  const ccAddresses: EmailAddress[] = parsed.cc
    ? (Array.isArray(parsed.cc) ? parsed.cc : [parsed.cc]).flatMap((t) =>
        t.value.map((addr) => ({ name: addr.name, address: addr.address || '' }))
      )
    : [];

  const replyToAddresses: EmailAddress[] = parsed.replyTo
    ? (Array.isArray(parsed.replyTo) ? parsed.replyTo : [parsed.replyTo]).flatMap((t) =>
        t.value.map((addr) => ({ name: addr.name, address: addr.address || '' }))
      )
    : [];

  const attachments: Attachment[] = parsed.attachments.map((att) => ({
    filename: att.filename || 'attachment',
    contentType: att.contentType,
    size: att.size,
    contentId: att.contentId,
    content: att.content.toString('base64'),
  }));

  return {
    id: parsed.messageId || `${uid}`,
    uid,
    messageId: parsed.messageId || '',
    subject: parsed.subject || '(No Subject)',
    from: fromAddress,
    to: toAddresses,
    cc: ccAddresses,
    replyTo: replyToAddresses.length > 0 ? replyToAddresses : undefined,
    date: parsed.date?.toISOString() || new Date().toISOString(),
    text: parsed.text,
    html: parsed.html || undefined,
    attachments,
    flags: [],
    folder,
    seen: false,
    flagged: false,
    answered: false,
    hasAttachments: attachments.length > 0,
  };
}

export async function markAsRead(
  email: string,
  password: string,
  folder: string,
  uid: number
): Promise<void> {
  const imap = createImapConnection(email, password);

  try {
    await connectImap(imap);

    return new Promise((resolve, reject) => {
      imap.openBox(folder, false, (err) => {
        if (err) {
          reject(err);
          return;
        }

        imap.addFlags(uid, ['\\Seen'], (flagErr) => {
          disconnectImap(imap);
          if (flagErr) {
            reject(flagErr);
          } else {
            resolve();
          }
        });
      });
    });
  } catch (error) {
    disconnectImap(imap);
    throw error;
  }
}

export async function deleteEmail(
  email: string,
  password: string,
  folder: string,
  uid: number
): Promise<void> {
  const imap = createImapConnection(email, password);

  try {
    await connectImap(imap);

    return new Promise((resolve, reject) => {
      imap.openBox(folder, false, (err) => {
        if (err) {
          reject(err);
          return;
        }

        imap.addFlags(uid, ['\\Deleted'], (flagErr) => {
          if (flagErr) {
            disconnectImap(imap);
            reject(flagErr);
            return;
          }

          imap.expunge((expungeErr) => {
            disconnectImap(imap);
            if (expungeErr) {
              reject(expungeErr);
            } else {
              resolve();
            }
          });
        });
      });
    });
  } catch (error) {
    disconnectImap(imap);
    throw error;
  }
}

export async function moveEmail(
  email: string,
  password: string,
  fromFolder: string,
  toFolder: string,
  uid: number
): Promise<void> {
  const imap = createImapConnection(email, password);

  try {
    await connectImap(imap);

    return new Promise((resolve, reject) => {
      imap.openBox(fromFolder, false, (err) => {
        if (err) {
          reject(err);
          return;
        }

        imap.move(uid, toFolder, (moveErr) => {
          disconnectImap(imap);
          if (moveErr) {
            reject(moveErr);
          } else {
            resolve();
          }
        });
      });
    });
  } catch (error) {
    disconnectImap(imap);
    throw error;
  }
}

export async function toggleFlag(
  email: string,
  password: string,
  folder: string,
  uid: number,
  flag: string,
  add: boolean
): Promise<void> {
  const imap = createImapConnection(email, password);

  try {
    await connectImap(imap);

    return new Promise((resolve, reject) => {
      imap.openBox(folder, false, (err) => {
        if (err) {
          reject(err);
          return;
        }

        const method = add ? imap.addFlags.bind(imap) : imap.delFlags.bind(imap);
        method(uid, [flag], (flagErr: Error | null) => {
          disconnectImap(imap);
          if (flagErr) {
            reject(flagErr);
          } else {
            resolve();
          }
        });
      });
    });
  } catch (error) {
    disconnectImap(imap);
    throw error;
  }
}
