// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#pragma once

#include "pch.h"
#include "LangStrings.h"
#include "FileWatcherControl.h"
#include "FilePermsControl.h"
#include "FileTopControl.h"
#include "FileChangeControl.h"
#include "FileDupeControl.h"
#include "FileSearchControl.h"
#include "FileTreeControl.h"
#include "ControlView.h"

class CFileTreeView final : public CControlViewT<CFileTreeControl>
{
public:
    void RefreshPercentages() { GetControl().Invalidate(); }
    void OnUpdate(CWnd* sender, MODEL_CHANGE change, CItem* item) override;

protected:
    void InitializeColumns() override;
};

class CFileWatcherView final : public CControlViewT<CFileWatcherControl, LVS_SINGLESEL>
{
protected:
    void InitializeColumns() override;
};

class CFilePermsView final : public CControlViewT<CFilePermsControl>
{
protected:
    void InitializeColumns() override;
};

class CFileTopView final : public CControlViewT<CFileTopControl>
{
protected:
    void InitializeColumns() override;
};

class CFileDupeView final : public CControlViewT<CFileDupeControl>
{
protected:
    void InitializeColumns() override;
};

class CFileSearchView final : public CControlViewT<CFileSearchControl>
{
protected:
    void InitializeColumns() override;
};

// Change list with a band on top: "Compare with: [earlier scan]" and the fixed latest scan.
template<typename Control>
class CFileChangeViewT : public MessageTarget<CFileChangeViewT<Control>, CControlViewT<Control>>
{
public:
    void RefreshChanges() { this->GetControl().Refresh(); }

    static std::span<const RouteEntry> Routes()
    {
        static constexpr std::array entries
        {
            Route::Control<&CFileChangeViewT::OnBaselineSelected>(CBN_SELCHANGE, IDC_CHANGE_BASELINE),
        };
        return entries;
    }

protected:
    void CreateBand() override
    {
        constexpr RECT rect = { 0, 0, 0, 0 };
        m_compareLabel.Create(Localization::Lookup(IDS_CHANGES_COMPARE).c_str(),
            WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE | SS_ENDELLIPSIS, rect, this);
        m_combo.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST,
            rect, this, IDC_CHANGE_BASELINE);
        m_latestLabel.Create(L"", WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE | SS_ENDELLIPSIS, rect, this);
        this->GetControl().SetRefreshedCallback([this] { FillBand(); });
    }

    int LayoutBand(const int cx) override
    {
        const int height = ScaleForDpi(30);
        if (m_combo.m_hWnd == nullptr) return height;
        const int margin = ScaleForDpi(6);
        const int labelWidth = ScaleForDpi(100);
        const int comboWidth = std::min(ScaleForDpi(430), std::max(ScaleForDpi(120), cx / 2));
        const int comboHeight = ScaleForDpi(300);   // includes the dropped-down list
        m_compareLabel.MoveWindow(margin, 0, labelWidth, height);
        m_combo.MoveWindow(margin + labelWidth, (height - ScaleForDpi(22)) / 2, comboWidth, comboHeight);
        const int latestLeft = margin + labelWidth + comboWidth + margin;
        m_latestLabel.MoveWindow(latestLeft, 0, std::max(0, cx - latestLeft - margin), height);
        return height;
    }

    void FillBand()
    {
        if (m_combo.m_hWnd == nullptr) return;
        auto& control = this->GetControl();
        const auto choices = control.GetScanChoices();
        m_choiceTimes.clear();
        while (m_combo.GetCount() > 0) m_combo.DeleteString(0);
        int selected = 0;
        for (const auto& [index, choice] : std::views::enumerate(choices))
        {
            m_combo.AddString(choice.text);
            m_choiceTimes.push_back(choice.time);
            if (control.GetBaseline() == choice.time) selected = static_cast<int>(index);
        }
        if (!choices.empty()) m_combo.SetCurSel(selected);
        m_combo.EnableWindow(!choices.empty());
        m_latestLabel.SetText(control.GetLatestText());
    }

    void OnBaselineSelected()
    {
        const int selected = m_combo.GetCurSel();
        if (selected < 0 || static_cast<std::size_t>(selected) >= m_choiceTimes.size()) return;
        // The first entry is the default: it keeps following "the scan before the newest".
        this->GetControl().SetBaseline(selected == 0 ? 0 : m_choiceTimes[static_cast<std::size_t>(selected)]);
    }

    CStatic m_compareLabel;
    CComboBox m_combo;
    CStatic m_latestLabel;
    std::vector<std::int64_t> m_choiceTimes;
};

class CFileChangeSizeView final : public CFileChangeViewT<CFileChangeSizeControl>
{
protected:
    void InitializeColumns() override;
};

class CFileChangeFilesView final : public CFileChangeViewT<CFileChangeFilesControl>
{
protected:
    void InitializeColumns() override;
};
