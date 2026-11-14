// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#pragma once

#include "pch.h"
#include "WinDirStatPane.h"
#include "ExtensionListControl.h"

//
// CExtensionView. The upper right view, which shows the extensions and their
// cushion colors.
//
class CExtensionView final : public MessageTarget<CExtensionView, CWinDirStatPane>
{
public:
    CExtensionView();

    ~CExtensionView() override = default;

    void SysColorChanged();
    bool IsShowTypes() const { return m_showTypes; }
    void ShowTypes(bool show);

    void SetHighlightExtension(const std::wstring& ext, bool unregistered = false);

    void OnUpdate(CWnd* sender, MODEL_CHANGE change, CItem* item) override;
    void SetSelection();

    // Fills the list with the extensions of the whole scan or, when a folder other than the
    // root is selected, only with the files below that folder.
    void RefreshData();
    const CItem* GetScopeItem() const;

    const CItem* m_scope = nullptr; // Folder the list currently describes; null for the whole scan
    bool m_showTypes = true; // Whether this view shall be shown (F8 option)
    CExtensionListControl m_extensionListControl; // The list control

static std::span<const RouteEntry> Routes();

protected:
    int OnCreate(LPCREATESTRUCT lpCreateStruct);
    void OnSize(UINT nType, int cx, int cy);
    void OnSetFocus(WindowRef pOldWnd);
    bool OnEraseBkgnd(CDC*) { return true; }
};

inline std::span<const RouteEntry> CExtensionView::Routes()
{
    static constexpr std::array entries
    {
        Route::Window<&OnCreate>(WM_CREATE),
        Route::Window<&OnEraseBkgnd>(WM_ERASEBKGND),
        Route::Window<&OnSize>(WM_SIZE),
        Route::Window<&OnSetFocus>(WM_SETFOCUS),
    };
    return entries;
}
