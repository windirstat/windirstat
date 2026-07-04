// WinDirStat - Directory Statistics
// Copyright © WinDirStat Team
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// at your option any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
//

#include "pch.h"
#include "FileWatcherControl.h"
#include "FinderBasic.h"

CFileWatcherControl::CFileWatcherControl()
    : CTreeListControl(COptions::WatcherColumnOrder.Ptr(), COptions::WatcherColumnWidths.Ptr(), LF_WATCHERLIST, false)
{
    SetOwnsItems(true);
    m_singleton = this;
}

CFileWatcherControl::~CFileWatcherControl()
{
    StopMonitoring();
    ClearPendingItems();
    m_singleton = nullptr;
}

BEGIN_MESSAGE_MAP(CFileWatcherControl, CTreeListControl)
    ON_WM_DESTROY()
    ON_MESSAGE(WM_WATCHER_CHANGE, OnWatcherChange)
END_MESSAGE_MAP()

void CFileWatcherControl::OnDestroy()
{
    StopMonitoring();
    ClearPendingItems();
    DeleteAllItems();

    CWdsListControl::OnDestroy();
}

void CFileWatcherControl::StartMonitoring()
{
    StopMonitoring();

    const auto* model = CWinDirStatModel::Get();
    if (!model->HasRootItem()) return;

    if (CItem* root = model->GetRootItem(); root != nullptr)
    {
        auto children = root->IsTypeOrFlag(IT_MYCOMPUTER) ?
            std::span<CItem* const>(root->GetChildren()) : std::span<CItem* const>(&root, 1);
        for (const auto& child : children)
        {
            m_watchThreads.emplace_back([this, path = child->GetPath()](const std::stop_token& stopToken)
            {
                WatchDirectory(path, stopToken);
            });
        }
    }
}

void CFileWatcherControl::StopMonitoring()
{
    for (auto& thread : m_watchThreads) thread.request_stop();
    m_watchThreads.clear();
}

void CFileWatcherControl::WatchDirectory(const std::wstring& path, const std::stop_token& stopToken)
{
    const SmartPointer hDir(CloseHandle, CreateFileW(FinderBasic::MakeLongPathCompatible(path).c_str(),
        FILE_LIST_DIRECTORY, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
        OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, nullptr));
    if (hDir == INVALID_HANDLE_VALUE) return;

    std::wstring pathWithSlash = path;
    if (pathWithSlash.back() != L'\\') pathWithSlash += L'\\';

    const SmartPointer hEvent(CloseHandle, CreateEvent(nullptr, TRUE, FALSE, nullptr));
    const SmartPointer hStopEvent(CloseHandle, CreateEvent(nullptr, TRUE, FALSE, nullptr));
    OVERLAPPED overlapped{ .hEvent = hEvent };
    std::vector<BYTE> buffer(64ul * 1024ul);

    std::stop_callback stopCallback(stopToken, [handle = hDir.Get(), stop = hStopEvent.Get()]()
    {
        SetEvent(stop);
        CancelIoEx(handle, nullptr);
    });

    const std::array<HANDLE, 2> waitHandles = { hEvent, hStopEvent };

    for (; !stopToken.stop_requested(); ResetEvent(overlapped.hEvent))
    {
        if (!ReadDirectoryChangesW(hDir, buffer.data(), static_cast<DWORD>(buffer.size()), TRUE,
            FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE,
            nullptr, &overlapped, nullptr))
        {
            if (GetLastError() != ERROR_IO_PENDING) break;
        }

        if (WaitForMultipleObjects(static_cast<DWORD>(waitHandles.size()), waitHandles.data(), FALSE, INFINITE) != WAIT_OBJECT_0) break;
        if (stopToken.stop_requested()) break;

        DWORD bytesReturned = 0;
        if (GetOverlappedResult(hDir, &overlapped, &bytesReturned, FALSE) == 0)
        {
            if (GetLastError() == ERROR_OPERATION_ABORTED) break;
            continue;
        }

        if (bytesReturned == 0) continue;

        for (auto pNotify = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(buffer.data());
            pNotify; pNotify = pNotify->NextEntryOffset ?
            ByteOffset<FILE_NOTIFY_INFORMATION>(pNotify, pNotify->NextEntryOffset) : nullptr)
        {
            AddChange(pathWithSlash + std::wstring(pNotify->FileName, pNotify->FileNameLength / sizeof(WCHAR)), pNotify->Action);
        }
    }
}

void CFileWatcherControl::AddChange(const std::wstring& path, const DWORD action)
{
    if (GetSafeHwnd() == nullptr) return;

    // Ignore modified events for directories as changes will be captured via their children
    WIN32_FILE_ATTRIBUTE_DATA fileAttr{};
    GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &fileAttr);
    if (action == FILE_ACTION_MODIFIED &&
        (fileAttr.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) return;

    FILETIME fileTime{};
    GetSystemTimeAsFileTime(&fileTime);

    static auto verbs = SplitString(Localization::Lookup(IDS_WATCHER_VERBS), L',');
    if (action == 0 || action > verbs.size()) return;

    const auto item = new CWatcherItem(path, verbs[action - 1], fileTime,
        ULARGE_INTEGER{ .u = { fileAttr.nFileSizeLow, fileAttr.nFileSizeHigh } }.QuadPart, fileAttr.dwFileAttributes);
    m_pendingItems.push(item);
    PostMessage(WM_WATCHER_CHANGE, 0, 0);
}

void CFileWatcherControl::ClearResults()
{
    ClearPendingItems();
    DeleteAllItems();
    Invalidate();
}

void CFileWatcherControl::ClearPendingItems()
{
    CWatcherItem* item = nullptr;
    while (m_pendingItems.pop(item))
    {
        delete item;
    }
}

LRESULT CFileWatcherControl::OnWatcherChange(WPARAM, LPARAM)
{
    std::vector<CWdsListItem*> items;
    CWatcherItem* item = nullptr;
    while (m_pendingItems.pop(item))
    {
        item->SetVisible(this, true);
        items.push_back(item);
    }

    if (items.empty()) return 0;

    const CSetRedrawLock lock(this);
    InsertListItem(GetItemCount(), items);
    SortItems();

    // Keep the most recent change in view when autoscroll is enabled
    if (COptions::WatcherAutoScroll)
    {
        EnsureItemVisible(static_cast<CTreeListItem*>(items.back()));
    }
    return 0;
}

HICON CWatcherItem::GetIcon()
{
    // No icon to return if not visible yet
    if (m_visualInfo == nullptr)
    {
        return nullptr;
    }

    // Return previously cached value
    if (m_visualInfo->icon != nullptr)
    {
        return m_visualInfo->icon;
    }

    // Fetch all icons
    CDirStatApp::Get()->GetIconHandler()->DoAsyncShellInfoLookup(std::make_tuple(this,
        m_visualInfo->control, m_item->GetPath(), m_item->GetAttributes(), &m_visualInfo->icon, nullptr));
    return nullptr;
}
