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

#pragma once

#include "Finder.h"

struct MtpDevice
{
    std::wstring path;
    std::wstring name;
};

class FinderMtp final : public Finder
{
public:
    FinderMtp() = default;
    ~FinderMtp() override = default;

    FinderMtp(const FinderMtp&) = delete;
    FinderMtp& operator=(const FinderMtp&) = delete;

    bool FindNext() override;
    bool FindFile(const CItem* item) override;
    DWORD GetAttributes() const override { return m_attributes; }
    ULONGLONG GetFileSizePhysical() const override { return m_size; }
    ULONGLONG GetFileSizeLogical() const override { return m_size; }
    FILETIME GetLastWriteTime() const override { return m_lastWrite; }
    std::wstring GetFilePath() const override { return m_path; }
    std::wstring GetFileName() const override { return m_name; }
    std::wstring GetShellPath() const override { return m_shellPath; }
    PCIDLIST_ABSOLUTE GetShellPidl() const override { return m_shellPidl.Get(); }
    ULONGLONG GetIndex() const override { return 0; }
    DWORD GetReparseTag() const override { return 0; }
    bool IsReserved() const override { return false; }

    static bool IsPath(std::wstring_view path) noexcept;
    static std::wstring StripPrefix(std::wstring_view path);
    static std::vector<MtpDevice> GetDevices();
    static bool GetDriveInfo(const std::wstring& path, std::wstring& name,
        ULONGLONG& total, ULONGLONG& freeBytes);
    static std::wstring GetDisplayName(const std::wstring& path);
    static bool DoesFileExist(const std::wstring& path);
    static bool DoesFileExist(const CItem* item);

    static ULONGLONG RegisterPath(std::wstring path, std::wstring shellPath = {},
        PCIDLIST_ABSOLUTE shellPidl = nullptr);
    static void UnregisterPath(ULONGLONG token) noexcept;
    static std::wstring GetPath(ULONGLONG token);
    static std::wstring GetShellPath(ULONGLONG token);
    static PIDLIST_ABSOLUTE CloneShellPidl(ULONGLONG token);
    static bool HasShellIdentity(ULONGLONG token);

private:
    static constexpr ULONGLONG PathTokenBit = ULONGLONG{ 1 } << 63;
    using PidlPointer = SmartPointer<PIDLIST_ABSOLUTE, decltype(&CoTaskMemFree)>;

    struct RegisteredPath
    {
        RegisteredPath(std::wstring path, std::wstring shellPath, PCIDLIST_ABSOLUTE shellPidl) :
            // Keep an owned PIDL clone alongside the parsing paths for reliable Shell resolution.
            path(std::move(path)), shellPath(std::move(shellPath)),
            shellPidl(CoTaskMemFree, shellPidl ? ILCloneFull(shellPidl) : nullptr)
        {
        }

        std::wstring path;
        std::wstring shellPath;
        PidlPointer shellPidl;
    };

    inline static std::mutex s_pathMutex;
    inline static std::unordered_map<ULONGLONG, RegisteredPath> s_paths;
    inline static ULONGLONG s_nextPathToken = PathTokenBit;

    ComApartmentScope m_com;
    CComPtr<IEnumShellItems> m_items;
    std::wstring m_basePath;
    std::wstring m_path;
    std::wstring m_name;
    std::wstring m_shellPath;
    PidlPointer m_shellPidl{ CoTaskMemFree };
    ULONGLONG m_size = 0;
    FILETIME m_lastWrite = {};
    DWORD m_attributes = INVALID_FILE_ATTRIBUTES;
};
