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

class CExtensionView;

//
// CExtensionListControl.
//
class CExtensionListControl final : public COwnerDrawnListControl
{
protected:
    // Columns
    enum ListColumns : std::uint8_t
    {
        COL_EXT_EXTENSION,
        COL_EXT_COLOR,
        COL_EXT_DESCRIPTION,
        COL_EXT_BYTES,
        COL_EXT_BYTESPERCENT,
        COL_EXT_FILES
    };

    // CListItem. The items of the CExtensionListControl.
    class CListItem final : public COwnerDrawnListItem
    {
    public:
        CListItem(CExtensionListControl* list, const std::wstring & extension, const SExtensionRecord& r);
        ~CListItem() override = default;

        bool DrawSubItem(int subitem, CDC* pdc, CRect rc, UINT state, int* width, int* focusLeft) override;
        std::wstring GetText(int subitem) const override;

        std::wstring GetExtension() const;
        HICON GetIcon() override;
        int Compare(const COwnerDrawnListItem* baseOther, int subitem) const override;

    private:
        void DrawColor(CDC* pdc, CRect rc, UINT state, int* width) const;

        std::wstring GetDescription() const;
        std::wstring GetBytesPercent() const;

        double GetBytesFraction() const;

        std::wstring m_extension;
        std::wstring m_description;
        CExtensionListControl* m_driveList;
        HICON m_icon = nullptr;
        ULONGLONG m_bytes = 0;
        ULONGLONG m_files = 0;
        COLORREF m_color = 0;
    };

public:
    CExtensionListControl(CExtensionView* extensionView);
    bool GetAscendingDefault(int subitem) override;
    void Initialize();
    void SetExtensionData(const CExtensionData* ed);
    void SetRootSize(ULONGLONG totalBytes);
    ULONGLONG GetRootSize() const;
    void SelectExtension(const std::wstring& ext);
    std::wstring GetSelectedExtension() const;

protected:
    CListItem* GetListItem(int i) const;

    CBitmap m_searchBitmap;
    CExtensionView* m_extensionView;
    ULONGLONG m_rootSize = 0;

    DECLARE_MESSAGE_MAP()
    afx_msg void OnLvnDeleteItem(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnSetFocus(CWnd* pOldWnd);
    afx_msg void OnLvnItemChanged(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
    afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);
    afx_msg void OnSearchExtension();
};
