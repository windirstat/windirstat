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
#include "FileWatcherControl.h"
#include "FileTopControl.h"
#include "FileDupeControl.h"
#include "FileSearchControl.h"
#include "FileTreeControl.h"
#include "ControlView.h"

class CFileTreeView final : public CControlView
{
protected:

    CTreeListControl& GetControl() override { return m_control; }
    const CTreeListControl& GetControl() const override { return m_control; }

    DECLARE_DYNCREATE(CFileTreeView)
    CFileTreeView() = default;
    ~CFileTreeView() override = default;

    void CreateColumns(bool all = false);
    void OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint) override;

    CFileTreeControl m_control;

    DECLARE_MESSAGE_MAP()
    afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
};

class CFileWatcherView final : public CControlView
{
protected:
    CTreeListControl& GetControl() override { return m_control; }
    const CTreeListControl& GetControl() const override { return m_control; }

    DECLARE_DYNCREATE(CFileWatcherView)
    CFileWatcherView() = default;
    ~CFileWatcherView() override = default;

    CFileWatcherControl m_control;

    DECLARE_MESSAGE_MAP()
    afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
};

class CFileTopView final : public CControlView
{
protected:
    DECLARE_DYNCREATE(CFileTopView)
    CFileTopView() = default;
    ~CFileTopView() override = default;

    CTreeListControl& GetControl() override { return m_control; }
    const CTreeListControl& GetControl() const override { return m_control; }

    CFileTopControl m_control;

    DECLARE_MESSAGE_MAP()
    afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
};

class CFileDupeView final : public CControlView
{
protected:

    DECLARE_DYNCREATE(CFileDupeView)
    CFileDupeView() = default;
    ~CFileDupeView() override = default;

    CTreeListControl& GetControl() override { return m_control; }
    const CTreeListControl& GetControl() const override { return m_control; }

    CFileDupeControl m_control;

    DECLARE_MESSAGE_MAP()
    afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
};

class CFileSearchView final : public CControlView
{
protected:

    DECLARE_DYNCREATE(CFileSearchView)
    CFileSearchView() = default;
    ~CFileSearchView() override = default;

    CTreeListControl& GetControl() override { return m_control; }
    const CTreeListControl& GetControl() const override { return m_control; }

    CFileSearchControl m_control;

    DECLARE_MESSAGE_MAP()
    afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
};
