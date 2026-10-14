// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#pragma once

#include "Finder.h"
#include "StorageSource.h"

class CProgressDlg;

struct RemoteDeleteResult
{
    std::wstring error;
    std::vector<CItem*> refresh;
};

class FinderRemote final : public Finder
{
public:
    FinderRemote(std::shared_ptr<StorageSource> source, BlockingQueue<CItem*>* queue) :
        m_source(std::move(source)), m_queue(queue) {}
    bool FindFile(const CItem* item) override;
    bool FindNext() override;
    DWORD GetAttributes() const override
    {
        return Current().directory ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
    }

    ULONGLONG GetFileSizePhysical() const override { return Current().size; }
    ULONGLONG GetFileSizeLogical() const override { return Current().size; }
    FILETIME GetLastWriteTime() const override { return Current().modified; }
    FILETIME GetDirectoryTime() const { return m_directoryTime; }
    std::wstring GetFilePath() const override;
    std::wstring_view GetFileName() const override { return m_name; }
    std::wstring GetRelativeKey() const
        { return Current().key.size() < m_target.prefix.size() ? L"" : Current().key.substr(m_target.prefix.size()); }
    void SetEntryName(std::wstring name) { m_name = std::move(name); }
    const ScanResult& GetResult() const noexcept override { return m_result; }
    void UseFlatListing(bool flat) noexcept { m_flat = flat; }
    ULONGLONG GetIndex() const override { return 0; }
    DWORD GetReparseTag() const override { return 0; }
    bool IsReserved() const override { return false; }
    bool HasIgnoredStream() const override { return false; }

    static RemoteDeleteResult Delete(std::span<CItem* const> items, CProgressDlg* progress, bool emptyOnly = false);

private:
    struct Entry
    {
        std::wstring key;
        ULONGLONG size = 0;
        FILETIME modified{};
        bool directory = false;
    };
    struct Page
    {
        std::vector<Entry> entries;
        std::wstring nextToken;
        FILETIME directoryTime{};
    };
    const Entry& Current() const { return m_entries[m_position]; }
    void LoadPage(bool recursive = false);
    Page ReadS3Page(bool recursive);
    Page ReadAzurePage(bool recursive, bool probe = false);
    Page ReadWebDavPage();

    std::shared_ptr<StorageSource> m_source;
    BlockingQueue<CItem*>* m_queue;
    CProgressDlg* m_progress = nullptr;
    RemoteTarget m_target;
    std::wstring m_name;
    ScanResult m_result;
    bool m_flat = false;
    bool m_published = false;
    std::vector<Entry> m_entries;
    size_t m_position = 0;
    std::wstring m_token;
    std::unordered_set<std::wstring> m_tokens;
    std::unordered_set<std::wstring> m_directories;
    bool m_more = true;
    FILETIME m_directoryTime{};
};
