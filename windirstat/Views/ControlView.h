// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#pragma once

#include "pch.h"
#include "WinDirStatPane.h"
#include "TreeListControl.h"

class CControlView : public MessageTarget<CControlView, CWinDirStatPane>
{
public:
    void SysColorChanged();

    CControlView() = default;
    ~CControlView() override = default;

    virtual CTreeListControl& GetControl() = 0;
    virtual const CTreeListControl& GetControl() const = 0;

    int InsertCol(const std::wstring_view& colName, int nFormat, int nWidth, int nSubItem);
    void OnUpdate(CWnd* sender, MODEL_CHANGE change, CItem* item) override;

static std::span<const RouteEntry> Routes();

protected:
    static constexpr DWORD DefaultControlStyle =
        LVS_OWNERDATA | WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SHOWSELALWAYS;

    virtual void InitializeColumns() = 0;
    virtual DWORD GetControlStyle() const = 0;

    int OnCreate(LPCREATESTRUCT lpCreateStruct);
    void OnSize(UINT nType, int cx, int cy);
    bool OnEraseBkgnd(CDC*) { return true; }
    void OnSetFocus(WindowRef pOldWnd);
    void OnLvnItemChanged(NMHDR* pNMHDR, LRESULT* pResult);
    void OnUpdatePopupToggle(CCmdUI* pCmdUI);
    void OnPopupToggle();
};

inline std::span<const RouteEntry> CControlView::Routes()
{
    static constexpr std::array entries
    {
        Route::Window<&OnCreate>(WM_CREATE),
        Route::Window<&OnSize>(WM_SIZE),
        Route::Window<&OnEraseBkgnd>(WM_ERASEBKGND),
        Route::Window<&OnSetFocus>(WM_SETFOCUS),
        Route::Notify<&OnLvnItemChanged>(LVN_ITEMCHANGED, ID_WDS_CONTROL),
        Route::Update<&OnUpdatePopupToggle>(ID_POPUP_TOGGLE),
        Route::Command<&OnPopupToggle>(ID_POPUP_TOGGLE),
    };
    return entries;
}

template<typename Control, DWORD AdditionalStyle = 0>
class CControlViewT : public CControlView
{
public:
    Control& GetControl() final { return m_control; }
    const Control& GetControl() const final { return m_control; }

protected:
    DWORD GetControlStyle() const final { return DefaultControlStyle | AdditionalStyle; }

private:
    Control m_control;
};
