'use client';

import { useState } from 'react';
import Link from 'next/link';
import { usePathname } from 'next/navigation';
import { cn } from '@/lib/utils';
import { Folder } from '@/types';
import Button from './Button';

interface SidebarProps {
  folders: Folder[];
  onCompose: () => void;
  onLogout: () => void;
  userEmail: string;
}

const folderIcons: Record<string, string> = {
  inbox: '📥',
  sent: '📤',
  drafts: '📝',
  junk: '🗑️',
  trash: '🗑️',
  archive: '📦',
};

export default function Sidebar({ folders, onCompose, onLogout, userEmail }: SidebarProps) {
  const pathname = usePathname();
  const [collapsed, setCollapsed] = useState(false);

  const getFolderIcon = (folder: Folder) => {
    if (folder.specialUse && folderIcons[folder.specialUse]) {
      return folderIcons[folder.specialUse];
    }
    return '📁';
  };

  const getFolderPath = (folder: Folder) => {
    return `/mail/${encodeURIComponent(folder.path)}`;
  };

  return (
    <aside
      className={cn(
        'flex flex-col bg-gray-50 border-r border-gray-200 transition-all duration-200',
        collapsed ? 'w-16' : 'w-64'
      )}
    >
      {/* Header */}
      <div className="p-4 border-b border-gray-200">
        <div className="flex items-center justify-between">
          {!collapsed && (
            <h1 className="text-xl font-bold text-primary-600">Webmail</h1>
          )}
          <button
            onClick={() => setCollapsed(!collapsed)}
            className="p-2 rounded-lg hover:bg-gray-200 transition-colors"
            title={collapsed ? 'Expand' : 'Collapse'}
          >
            {collapsed ? '→' : '←'}
          </button>
        </div>
      </div>

      {/* Compose Button */}
      <div className="p-4">
        <Button
          onClick={onCompose}
          className={cn('w-full', collapsed && 'px-2')}
        >
          {collapsed ? '✏️' : 'Compose'}
        </Button>
      </div>

      {/* Folders */}
      <nav className="flex-1 overflow-y-auto px-2">
        <ul className="space-y-1">
          {folders.map((folder) => {
            const isActive = pathname === getFolderPath(folder);
            return (
              <li key={folder.path}>
                <Link
                  href={getFolderPath(folder)}
                  className={cn(
                    'flex items-center gap-3 px-3 py-2 rounded-lg transition-colors',
                    isActive
                      ? 'bg-primary-100 text-primary-700'
                      : 'text-gray-700 hover:bg-gray-200'
                  )}
                  title={folder.name}
                >
                  <span>{getFolderIcon(folder)}</span>
                  {!collapsed && (
                    <>
                      <span className="flex-1 truncate">{folder.name}</span>
                      {folder.unseen > 0 && (
                        <span className="bg-primary-600 text-white text-xs px-2 py-0.5 rounded-full">
                          {folder.unseen}
                        </span>
                      )}
                    </>
                  )}
                </Link>
              </li>
            );
          })}
        </ul>
      </nav>

      {/* User Info & Logout */}
      <div className="p-4 border-t border-gray-200">
        {!collapsed && (
          <p className="text-sm text-gray-500 truncate mb-2" title={userEmail}>
            {userEmail}
          </p>
        )}
        <Button
          variant="ghost"
          onClick={onLogout}
          className={cn('w-full', collapsed && 'px-2')}
        >
          {collapsed ? '🚪' : 'Logout'}
        </Button>
      </div>
    </aside>
  );
}
