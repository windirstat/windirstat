// ModalShellApi.h - Declaration of CModalShellApi
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

#pragma once

#include "ModalApiShuttle.h"

//
// CModalShellApi. Modal version of the shell functions
// EmptyRecycleBin and DeleteFile.
// 
// See comment on CModalApiShuttle.
//
class CModalShellApi final : public CModalApiShuttle
{
public:
    CModalShellApi();

    void DeleteFile(const std::wstring & fileName, bool toRecycleBin);

protected:
    bool DoOperation() override;

    bool DoDeleteItem();

    int m_Operation = 0;         // Enum specifying the desired operation
    std::wstring m_FileName;         // File name to be deleted
    bool m_ToRecycleBin = false; // True if file shall only be moved to the recycle bin
};
