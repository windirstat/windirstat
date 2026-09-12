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
    : MessageTarget(COptions::WatcherColumnOrder.Ptr(), COptions::WatcherColumnWidths.Ptr(), COptions::WatcherColumnVisibility.Ptr(), LF_WATCHERLIST, false)
{
    m_singleton = this;
}

CFileWatcherControl::~CFileWatcherControl()
{
    StopMonitoring();
    ClearPendingItems();
    m_singleton = nullptr;
}

void CFileWatcherControl::OnDestroy()
{
    StopMonitoring();
    ClearResults();

    CWdsListControl::OnDestroy();
}

void CFileWatcherControl::StartMonitoring()
{
    StopMonitoring();

    const auto* model = CWinDirStatModel::Get();
    if (!model->HasRootItem()) return;

    if (CItem* root = model->GetRootItem(); root != nullptr)
    {
        const auto children = root->IsTypeOrFlag(IT_MYCOMPUTER) ?
            std::span(root->GetChildren()) : std::span<CItem* const>(&root, 1);
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

    const SmartPointer hEvent(CloseHandle, CreateEvent(nullptr, true, false, nullptr));
    const SmartPointer hStopEvent(CloseHandle, CreateEvent(nullptr, true, false, nullptr));
    OVERLAPPED overlapped{ .hEvent = hEvent };
    std::vector<BYTE> buffer(64ul * 1024ul);

    std::stop_callback stopCallback(stopToken, [handle = hDir.Get(), stop = hStopEvent.Get()]
    {
        SetEvent(stop);
        CancelIoEx(handle, nullptr);
    });

    const std::array<HANDLE, 2> waitHandles = { hEvent, hStopEvent };

    for (; !stopToken.stop_requested(); ResetEvent(overlapped.hEvent))
    {
        if (!ReadDirectoryChangesW(hDir, buffer.data(), static_cast<DWORD>(buffer.size()), true,
            FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE,
            nullptr, &overlapped, nullptr))
        {
            if (GetLastError() != ERROR_IO_PENDING) break;
        }

        if (WaitForMultipleObjects(static_cast<DWORD>(waitHandles.size()), waitHandles.data(), false, INFINITE) != WAIT_OBJECT_0) break;
        if (stopToken.stop_requested()) break;

        DWORD bytesReturned = 0;
        if (GetOverlappedResult(hDir, &overlapped, &bytesReturned, false) == 0)
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
    if (Handle() == nullptr) return;

    // Ignore modified events for directories as changes will be captured via their children
    WIN32_FILE_ATTRIBUTE_DATA fileAttr{};
    GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &fileAttr);
    if (action == FILE_ACTION_MODIFIED &&
        (fileAttr.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) return;

    const FILETIME fileTime = CurrentSystemFileTime();

    static auto verbs = SplitString(Localization::Lookup(IDS_WATCHER_VERBS), L',');
    if (action == 0 || action > verbs.size()) return;

    const auto item = new CWatcherItem(path, verbs[action - 1], fileTime,
        ULARGE_INTEGER{ .u = { fileAttr.nFileSizeLow, fileAttr.nFileSizeHigh } }.QuadPart, fileAttr.dwFileAttributes);
    m_pendingItems.push(item);
    PostWatcherChange();
}

void CFileWatcherControl::PostWatcherChange()
{
    if (!m_changePending.exchange(true) && !PostMessage(WM_WATCHER_CHANGE))
        m_changePending = false;
}

void CFileWatcherControl::ClearResults()
{
    ClearPendingItems();
    DeleteAllItems();
    m_history.clear();
    Invalidate();
}

bool CFileWatcherControl::SetQuickFilter(const std::wstring& pattern)
{
    std::optional<std::wregex> filter;
    std::vector<CWdsListItem*> items;
    items.reserve(m_history.size());
    try
    {
        if (!pattern.empty()) filter.emplace(pattern, std::regex::icase | std::regex::optimize);
        for (const auto& item : m_history)
        {
            if (!filter || std::regex_search(item->GetLinkedItem()->GetPath(), *filter)) items.push_back(item.get());
        }
    }
    catch (const std::regex_error&)
    {
        return false;
    }

    const ScopedRedrawPause lock(this);
    const auto* selected = GetFirstSelectedItem();
    m_quickFilter = std::move(filter);
    DeleteAllItems();
    for (const auto& item : m_history) if (item->IsVisible()) item->SetVisible(this, false);
    for (auto* item : items) static_cast<CWatcherItem*>(item)->SetVisible(this, true);
    InsertSortedListItems(items);

    if (selected != nullptr && FindTreeItem(selected) != -1)
    {
        SelectItem(selected, false, true, true);
    }
    else if (COptions::WatcherAutoScroll && !items.empty())
    {
        EnsureItemVisible(static_cast<CTreeListItem*>(items.back()));
    }
    PostSelectionChanged();
    Invalidate();
    return true;
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
    constexpr std::size_t maxBatchSize = 1024;
    std::vector<CWdsListItem*> items;
    items.reserve(maxBatchSize);
    CWatcherItem* item = nullptr;
    for (std::size_t count = 0; count < maxBatchSize && m_pendingItems.pop(item); ++count)
    {
        m_history.emplace_back(item);
        try
        {
            if (m_quickFilter && !std::regex_search(item->GetLinkedItem()->GetPath(), *m_quickFilter)) continue;
        }
        catch (const std::regex_error&)
        {
            continue;
        }
        item->SetVisible(this, true);
        items.push_back(item);
    }

    if (!items.empty())
    {
        const ScopedRedrawPause lock(this);
        InsertSortedListItems(items);

        // Keep the most recent change in view when autoscroll is enabled
        if (COptions::WatcherAutoScroll)
        {
            EnsureItemVisible(static_cast<CTreeListItem*>(items.back()));
        }
    }
    m_changePending = false;
    if (!m_pendingItems.empty()) PostWatcherChange();
    return 0;
}

HICON CWatcherItem::GetIcon()
{
    // No icon to return if not visible yet
    if (m_visualInfo == nullptr)
    {
        return nullptr;
    }

    if (m_visualInfo->icon != nullptr)
    {
        return m_visualInfo->icon;
    }

    // Fetch all icons
    CDirStatApp::Get()->GetIconHandler()->DoAsyncShellInfoLookup(std::make_tuple(this,
        m_visualInfo->control, m_item->GetPath(), m_item->GetAttributes(), &m_visualInfo->icon, nullptr));
    return nullptr;
}
