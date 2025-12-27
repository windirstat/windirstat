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

#include "pch.h"

class COleFilterOverride final : public COleMessageFilter
{
public:

    bool m_defaultHandler = true;
    int m_refCounter = 0;
    std::mutex m_mutex;

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
        return (m_defaultHandler) ? COleMessageFilter::OnMessagePending(pMsg) : FALSE;
    }

    void SetDefaultHandler(bool defaultHandler)
    {
        if (AfxGetThread() == nullptr) return;
        std::scoped_lock guard(m_mutex);
        m_refCounter = (defaultHandler) ? (m_refCounter - 1) : (m_refCounter + 1);
        m_defaultHandler = m_refCounter == 0;
    }
};
