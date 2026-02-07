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

const std::unordered_map<uint8_t, uint8_t> CWatcherItem::s_columnMap =
{
    { COL_ITEMWATCH_TIME, COL_LAST_CHANGE },
    { COL_ITEMWATCH_NAME, COL_NAME },
    { COL_ITEMWATCH_SIZE_LOGICAL, COL_SIZE_LOGICAL }
};

CFileWatcherControl* CFileWatcherControl::m_singleton = nullptr;

CFileWatcherControl::CFileWatcherControl()
    : CTreeListControl(COptions::WatcherColumnOrder.Ptr(), COptions::WatcherColumnWidths.Ptr())
{
    m_singleton = this;
}

CFileWatcherControl::~CFileWatcherControl()
{
    StopMonitoring();
    m_singleton = nullptr;
}

BEGIN_MESSAGE_MAP(CFileWatcherControl, CTreeListControl)
    ON_WM_DESTROY()
    ON_WM_SETFOCUS()
END_MESSAGE_MAP()

void CFileWatcherControl::OnDestroy()
{
    StopMonitoring();
    DeleteAllItems();

    COwnerDrawnListControl::OnDestroy();
}

void CFileWatcherControl::OnSetFocus(CWnd* pOldWnd)
{
    CTreeListControl::OnSetFocus(pOldWnd);
    if (CMainFrame::Get()) CMainFrame::Get()->SetLogicalFocus(LF_WATCHERLIST);
}

void CFileWatcherControl::StartMonitoring()
{
    StopMonitoring();

    const auto* doc = CDirStatDoc::Get();
    if (!doc || !doc->HasRootItem()) return;

    if (CItem* root = doc->GetRootItem(); root != nullptr)
    {
        const auto& children = root->IsTypeOrFlag(IT_MYCOMPUTER) ? root->GetChildren() : std::vector{ root };
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
    m_watchThreads.clear();
}

bool CFileWatcherControl::IsMonitoring() const
{
    return !m_watchThreads.empty();
}

void CFileWatcherControl::WatchDirectory(const std::wstring& path, const std::stop_token& stopToken)
{
    SmartPointer<HANDLE> hDir(CloseHandle, CreateFileW(FinderBasic::MakeLongPathCompatible(path).c_str(),
        FILE_LIST_DIRECTORY, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
        OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, nullptr));
    if (hDir == INVALID_HANDLE_VALUE) return;

    std::wstring pathWithSlash = path;
    if (pathWithSlash.back() != L'\\') pathWithSlash += L'\\';

    SmartPointer<HANDLE> hEvent(CloseHandle, CreateEvent(nullptr, TRUE, FALSE, nullptr));
    OVERLAPPED overlapped{ .hEvent = hEvent };
    std::vector<BYTE> buffer(64ul * 1024ul);

    std::stop_callback stopCallback(stopToken, [handle = hDir.Get()]() { CancelIoEx(handle, nullptr); });

    for (; !stopToken.stop_requested(); ResetEvent(overlapped.hEvent))
    {
        if (!ReadDirectoryChangesW(hDir, buffer.data(), static_cast<DWORD>(buffer.size()), TRUE,
            FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE,
            nullptr, &overlapped, nullptr))
        {
            if (GetLastError() != ERROR_IO_PENDING) break;
        }

        if (WaitForSingleObject(overlapped.hEvent, INFINITE) != WAIT_OBJECT_0) continue;
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

    static auto verbs = SplitString(Localization::Lookup(IDS_WATCHER_VERBS), L',');

    // Ignore modified events for directories as they will be depicted by changed to their children
    WIN32_FILE_ATTRIBUTE_DATA fileAttr{};
    GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &fileAttr);
    if (action == FILE_ACTION_MODIFIED &&
        (fileAttr.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) return;

    FILETIME fileTime{};
    GetSystemTimeAsFileTime(&fileTime);

    auto item = std::make_unique<CWatcherItem>(path, verbs[action - 1], fileTime,
        ULARGE_INTEGER{ .u = { fileAttr.nFileSizeLow, fileAttr.nFileSizeHigh } }.QuadPart, fileAttr.dwFileAttributes);
    InsertItem(GetItemCount(), item.get());

    m_items.emplace_back(std::move(item));
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

