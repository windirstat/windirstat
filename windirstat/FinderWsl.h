// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#pragma once

#include "pch.h"
#include "Finder.h"
#include "FinderBasic.h"

// A registered WSL 2 distribution and the virtual disk that holds its root filesystem.
struct WslDistro
{
    std::wstring name;
    std::wstring vhdPath;
};

// Loads WSL trees by running a static Linux helper inside the distribution, much as
// FinderNtfsContext loads a volume from its MFT; each tree is rooted at the item that
// requested it and is immutable once published.
class FinderWslContext final
{
    friend class FinderWsl;

    struct Record
    {
        ULONGLONG inode = 0;
        ULONGLONG size = 0;
        ULONGLONG allocated = 0;
        FILETIME modified{};
        size_t nameOffset = 0;
        USHORT nameLength = 0;
        BYTE type = 0;
        BYTE flags = 0;
    };

    struct Tree
    {
        ULONGLONG rootInode = 0;
        std::vector<Record> records;
        std::vector<wchar_t> names;
        std::unordered_map<ULONGLONG, std::vector<ULONG>> children;
        std::unordered_map<ULONGLONG, std::wstring> failures;
        ScanResult result{ ScanOutcome::Complete, {} };
    };

    std::shared_mutex m_lock;
    std::unordered_map<const CItem*, std::unique_ptr<Tree>> m_trees;
    std::unordered_map<const CItem*, FinderBasicContext> m_shareContexts;

    static std::unique_ptr<Tree> Load(const std::wstring& path, BlockingQueue<CItem*>* queue, std::wstring& error);

public:

    // Returns the tree listing this folder, loading one rooted at it when no loaded tree does;
    // null means the helper failed for a selected distribution, whose folders need basic enumeration.
    const Tree* Resolve(CItem* item, BlockingQueue<CItem*>* queue);

    // Returns the share details of the WSL tree rooted at the item, apart from the volume that holds its disk.
    FinderBasicContext* GetShareContext(const CItem* root);
};

class FinderWsl final : public Finder
{
    FinderWslContext* m_master = nullptr;
    const FinderWslContext::Tree* m_tree = nullptr;
    const FinderWslContext::Record* m_record = nullptr;
    const ULONG* m_position = nullptr;
    const ULONG* m_end = nullptr;
    std::wstring m_base;
    ScanResult m_result{ ScanOutcome::Complete, {} };

public:

    // Index of folders that are mount points of other filesystems and are therefore not entered.
    static constexpr ULONGLONG MountIndex = ~0ull;

    explicit FinderWsl(FinderWslContext* master) : m_master(master) {}

    // Finds the tree that lists this folder; false means the folder needs basic enumeration.
    bool Resolve(CItem* item, BlockingQueue<CItem*>* queue);

    // Lists the children of the folder last resolved.
    bool FindFile(const CItem* item) override;
    bool FindNext() override;
    DWORD GetAttributes() const override;
    ULONGLONG GetFileSizePhysical() const override { return m_record->allocated; }
    ULONGLONG GetFileSizeLogical() const override { return m_record->size; }
    FILETIME GetLastWriteTime() const override { return m_record->modified; }
    std::wstring GetFilePath() const override;
    std::wstring_view GetFileName() const override
        { return { m_tree->names.data() + m_record->nameOffset, m_record->nameLength }; }
    ULONGLONG GetIndex() const override;
    DWORD GetReparseTag() const override;
    bool IsReserved() const override { return false; }
    bool HasIgnoredStream() const override { return false; }
    const ScanResult& GetResult() const noexcept override { return m_result; }

    // Distribution and path helpers
    static bool IsPath(std::wstring_view path) noexcept;
    static std::wstring GetRootPath(std::wstring_view distro) { return L"\\\\wsl.localhost\\" + std::wstring(distro); }
    static std::wstring GetDistroName(std::wstring_view path);
    static std::wstring GetPathKey(std::wstring_view path);
    static std::wstring ExpandShortNames(std::wstring path);
    static std::vector<WslDistro> GetDistros();
    static std::optional<WslDistro> FindDistroByVhd(std::wstring_view path);
    static std::optional<WslDistro> FindDistroByName(std::wstring_view name);
    static std::vector<std::wstring> GetRunningDistros();
    static bool IsDistroRunning(std::wstring_view name);
};
