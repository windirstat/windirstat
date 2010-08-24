// modalshellapi.h - Declaration of CModalShellApi
//
// WinDirStat - Directory Statistics
// Copyright (C) 2003-2005 Bernhard Seifert
// Copyright (C) 2004-2006, 2008 Oliver Schneider (assarbad.net)
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
// Author(s): - bseifert -> http://windirstat.info/contact/bernhard/
//            - assarbad -> http://windirstat.info/contact/oliver/
//

#ifndef __WDS_MODALSHELLAPI_H__
#define __WDS_MODALSHELLAPI_H__
#pragma once

#include "modalapishuttle.h"


//
// CModalShellApi. Modal version of the shell functions
// EmptyRecycleBin and DeleteFile.
// 
// See comment on CModalApiShuttle.
//
class CModalShellApi : public CModalApiShuttle
{
public:
    CModalShellApi();

    bool IsRecycleBinApiSupported();
    void EmptyRecycleBin();
    void DeleteFile(LPCTSTR fileName, bool toRecycleBin);

protected:
    virtual void DoOperation();

    void DoEmptyRecycleBin();
    void DoDeleteFile();

    CRecycleBinApi m_rbapi; // Dynamically linked shell32.dll functions
    int m_operation;        // Enum specifying the desired operation
    CString m_fileName;     // File name to be deleted
    bool m_toRecycleBin;    // True if file shall only be move to the recycle bin
};

#endif // __WDS_MODALSHELLAPI_H__
