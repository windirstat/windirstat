// MountPoints.h - Declaration of COleFilterOverride
//
// WinDirStat - Directory Statistics
// Copyright © WinDirStat Team
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

#include "stdafx.h"

#include <shared_mutex>

class COleFilterOverride final : public COleMessageFilter
{
public:

    bool m_DefaultHandler = true;
    int m_RefCounter = 0;
    std::shared_mutex m_Mutex;

    COleFilterOverride()
    {
        m_bEnableNotResponding = FALSE;
    }

    void RegisterFilter()
    {
        AfxOleGetMessageFilter()->Revoke();
        this->Register();
    }

    BOOL OnMessagePending(const MSG* pMsg) override
    {
        return (m_DefaultHandler) ? COleMessageFilter::OnMessagePending(pMsg) : FALSE;
    }

    void SetDefaultHandler(bool defaultHandler)
    {
        if (AfxGetThread() == nullptr) return;
        std::lock_guard guard(m_Mutex);
        m_RefCounter = (defaultHandler) ? (m_RefCounter - 1) : (m_RefCounter + 1);
        m_DefaultHandler = m_RefCounter == 0;
    }
};
