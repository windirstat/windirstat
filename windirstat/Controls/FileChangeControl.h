// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#pragma once

#include "pch.h"
#include "TreeListControl.h"
#include "ScanHistory.h"

enum ITEMCHANGECOLUMNS : std::uint8_t
{
    COL_ITEMCHANGE_NAME,
    COL_ITEMCHANGE_BAR,
    COL_ITEMCHANGE_PREVIOUS,
    COL_ITEMCHANGE_CURRENT,
    COL_ITEMCHANGE_DELTA,
    COL_ITEMCHANGE_PERCENT,
};

// One folder whose size (or file count) differs between the last two scans.
class CChangeItem final : public CTreeListItem
{
public:
    CChangeItem(std::wstring path, const ULONGLONG previous, const ULONGLONG current, const bool count)
        : m_path(std::move(path)), m_previous(previous), m_current(current), m_count(count) {}

    std::wstring GetText(int subitem) const override;
    int CompareSibling(const CTreeListItem* other, int subitem) const override;
    bool DrawSubItem(int subitem, CDC* pdc, CRect rc, UINT state, int* width, int* focusLeft) override;

    // Relative change (|change| / previous); infinite when the folder was empty before.
    double Ratio() const;
    // The bar of the row with the biggest finite ratio is full; set by the owning control.
    void SetBarScale(const double maxRatio) { m_barScale = maxRatio; }
    CTreeListItem* GetTreeListChild(int) const override { return nullptr; }
    int GetTreeListChildCount() const override { return 0; }
    CItem* GetLinkedItem() override;
    HICON GetIcon() override;

private:
    ULONGLONG Magnitude() const { return m_current > m_previous ? m_current - m_previous : m_previous - m_current; }
    std::wstring FormatValue(ULONGLONG value) const;

    std::unique_ptr<CItem> m_item;
    std::wstring m_path;
    ULONGLONG m_previous;
    ULONGLONG m_current;
    double m_barScale = 1.0;
    bool m_count;   // true: file counts, false: sizes in bytes
};

// Flat list of the folders with the biggest change between the last two scans of the
// current scan origin, read from the history file (see CScanHistory).
class CFileChangeControl : public CTreeListControl
{
public:
    CFileChangeControl(std::vector<int>* order, std::vector<int>* widths, std::vector<int>* visibility, bool count)
        : CTreeListControl(order, widths, visibility, static_cast<LOGICAL_FOCUS>(0), false), m_count(count) {}

    bool GetAscendingDefault(const int column) override { return column == COL_ITEMCHANGE_NAME; }
    void Refresh();
    void AfterDeleteAllItems() override;

    // Earlier scans that can be compared with, newest first (the newest scan itself is fixed).
    struct ScanChoice
    {
        std::int64_t time;
        std::wstring text;
    };
    std::vector<ScanChoice> GetScanChoices() const;
    std::wstring GetLatestText() const;
    std::int64_t GetBaseline() const { return m_baseline; }   // 0 = the scan before the newest
    void SetBaseline(const std::int64_t time) { m_baseline = time; Refresh(); }
    void SetRefreshedCallback(std::function<void()> callback) { m_refreshed = std::move(callback); }

protected:
    // Clicking "Previous" lets the user pick the earlier scan to compare with.
    bool OnHeaderClick(int subitem) override;

private:
    void ReleaseRows();
    void UpdatePreviousHeader(const std::vector<std::int64_t>& times);

    std::int64_t m_baseline = 0;   // 0 = the scan before the newest
    std::function<void()> m_refreshed;

    std::vector<std::unique_ptr<CChangeItem>> m_rows;
    bool m_count;
};

class CFileChangeSizeControl final : public CFileChangeControl
{
public:
    CFileChangeSizeControl() : CFileChangeControl(COptions::ChangeSizeColumnOrder.Ptr(),
        COptions::ChangeSizeColumnWidths.Ptr(), COptions::ChangeSizeColumnVisibility.Ptr(), false) {}
};

class CFileChangeFilesControl final : public CFileChangeControl
{
public:
    CFileChangeFilesControl() : CFileChangeControl(COptions::ChangeFilesColumnOrder.Ptr(),
        COptions::ChangeFilesColumnWidths.Ptr(), COptions::ChangeFilesColumnVisibility.Ptr(), true) {}
};
