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

#include "WinDirStat.h"

#include <functional>

//
// CModalApiShuttle. (Base class for CModalShellApi and CModalSendMail.)
//
// The SHFileOperation() function shows a modeless dialog, but we want
// them to be modal.
//
// My first approximation was:
//
// AfxGetMainWnd()->EnableWindow(false);
// Do the operation (SHFileOperation)
// AfxGetMainWnd()->EnableWindow(true);
//
// But when the operation window is destroyed, the system brings
// some other window to the foreground and WinDirStat ends up in the background.
// That's because it is still disabled at that moment.
//
// So my solution is this:
// First create an invisible (zero size) (but enabled) modal dialog,
// then do the operation in its OnInitDialog function
// and end the dialog.
//
class CModalApiShuttle : public CDialogEx
{
    DECLARE_DYNAMIC(CModalApiShuttle)

    CModalApiShuttle(const std::function<void()>& task, CWnd* pParent = nullptr);
    ~CModalApiShuttle() override = default;
    INT_PTR DoModal() override;

protected:
    enum : std::uint8_t { IDD = IDD_MODALAPISHUTTLE };

    BOOL OnInitDialog() override;
    DECLARE_MESSAGE_MAP()

    const std::function<void()> m_task;
};
