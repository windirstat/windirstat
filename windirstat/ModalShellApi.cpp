// ModalShellApi.cpp - Implementation of CModalShellApi
//
// WinDirStat - Directory Statistics
// Copyright (C) 2003-2005 Bernhard Seifert
// Copyright (C) 2004-2024 WinDirStat Team (windirstat.net)
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//

#include "stdafx.h"
#include "WinDirStat.h"
#include "ModalShellApi.h"
#include "SmartPointer.h"
#include "FileFind.h"

#include <VersionHelpers.h>
#include <filesystem>

namespace
{
    enum
    {
        DELETE_FILE = 1
    };
}

CModalShellApi::CModalShellApi() = default;

void CModalShellApi::DeleteFile(LPCWSTR fileName, bool toRecycleBin)
{
    m_operation    = DELETE_FILE;
    m_fileName     = fileName;
    m_toRecycleBin = toRecycleBin;

    DoModal();
}

bool CModalShellApi::DoOperation()
{
    if (m_operation == DELETE_FILE)
    {

        return DoDeleteItem();
    }

    return false;
}

bool CModalShellApi::DoDeleteItem()
{
    if (m_toRecycleBin)
    {
        // Determine flags to use for deletion
        const auto flags = FOF_NOCONFIRMATION | FOFX_EARLYFAILURE | FOFX_SHOWELEVATIONPROMPT |
            (IsWindows8OrGreater() ? (FOFX_ADDUNDORECORD | FOFX_RECYCLEONDELETE) : FOF_ALLOWUNDO);

        // Do deletion operation
        SmartPointer<LPITEMIDLIST> pidl(CoTaskMemFree, ILCreateFromPath(m_fileName));
        CComPtr<IShellItem> shellitem = nullptr;
        if (SHCreateItemFromIDList(pidl, IID_PPV_ARGS(&shellitem)) != S_OK) return false;
        
        ::CComPtr<IFileOperation> pFileOperation;
        if (FAILED(::CoCreateInstance(CLSID_FileOperation, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&pFileOperation))) ||
            FAILED(pFileOperation->SetOperationFlags(flags)) ||
            FAILED(pFileOperation->DeleteItem(shellitem, nullptr)) ||
            FAILED(pFileOperation->PerformOperations()))
        {
            return false;
        }
    }
    else
    {
        CStringW path = FileFindEnhanced::GetLongPathCompatible(m_fileName);
        std::error_code ec;
        std::filesystem::remove_all(std::filesystem::path(path.GetBuffer()), ec);
    }

    return true;
}
