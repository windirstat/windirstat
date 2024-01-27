// FileFindWDS.h - Declaration of CFileFindWDS
//
// WinDirStat - Directory Statistics
// Copyright (C) 2003-2005 Bernhard Seifert
// Copyright (C) 2004-2017 WinDirStat Team (windirstat.net)
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

#ifndef __WDS_FILEFINDWDS_H__
#define __WDS_FILEFINDWDS_H__
#pragma once
#include <afx.h> // Declaration of prototype for CFileFind

class CFileFindWDS : public CFileFind
{
public:
    DWORD GetAttributes() const;
    ULONGLONG GetCompressedLength(DWORD FileAttributes) const;
};

#endif // __WDS_FILEFINDWDS_H__
