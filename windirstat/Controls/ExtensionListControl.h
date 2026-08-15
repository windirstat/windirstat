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
class CExtensionListControl final : public MessageTarget<CExtensionListControl, CWdsListControl>
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
    class CListItem final : public CWdsListItem
    {
    public:
        CListItem(CExtensionListControl* list, std::wstring extension, const SExtensionRecord& r, bool aggregate = false);
        ~CListItem() override = default;

        bool DrawSubItem(int subitem, CDC* pdc, CRect rc, UINT state, int* width, int* focusLeft) override;
        std::wstring GetText(int subitem) const override;

        const std::wstring& GetExtension() const { return m_extension; }
        HICON GetIcon() override;
        int Compare(const CWdsListItem* baseOther, int subitem) const override;

        // True for the synthetic entry that aggregates all unregistered extensions.
        bool IsAggregate() const { return m_aggregate; }

    private:
        void DrawColor(CDC* pdc, CRect rc, UINT state, int* width) const;

        const std::wstring& GetDescription() const;
        std::wstring GetBytesPercent() const;

        double GetBytesFraction() const;

        std::wstring m_extension;
        std::wstring m_description;
        CExtensionListControl* m_extensionList;
        HICON m_icon = nullptr;
        ULONGLONG m_bytes = 0;
        ULONGLONG m_files = 0;
        COLORREF m_color = 0;
        bool m_aggregate = false;
    };

public:
    CExtensionListControl(CExtensionView* extensionView);
    bool GetAscendingDefault(int subitem) override;
    void Initialize();
    void SetExtensionData(const CExtensionData* ed);
    void SetRootSize(const ULONGLONG totalBytes) { m_rootSize = totalBytes; }
    ULONGLONG GetRootSize() const { return m_rootSize; }
    void SelectExtension(const std::wstring& ext);
    std::wstring GetSelectedExtension() const;

protected:
    CListItem* GetListItem(const int i) const { return static_cast<CListItem*>(GetItem(i)); }
    bool IsSelectedAggregate() const;
    void OnItemContextMenu(CPoint point) override;

    CBitmap m_searchBitmap;
    CExtensionView* m_extensionView;
    ULONGLONG m_rootSize = 0;

public:
    static std::span<const RouteEntry> Routes();

protected:
    void OnLvnDeleteItem(NMHDR* pNMHDR, LRESULT* pResult);
    void OnNMDblclk(NMHDR* pNMHDR, LRESULT* pResult);
    void OnSetFocus(CWnd* pOldWnd);
    void OnLvnItemChanged(NMHDR* pNMHDR, LRESULT* pResult) const;
    void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
    void OnSearchExtension() const;
    void OnExcludeExtension() const;
};

inline std::span<const RouteEntry> CExtensionListControl::Routes()
{
    using ThisClass = CExtensionListControl;
    static constexpr std::array entries
    {
        Route::ReflectNotify<&ThisClass::OnLvnDeleteItem>(LVN_DELETEITEM),
        Route::ReflectNotify<&ThisClass::OnNMDblclk>(NM_DBLCLK),
        Route::Window<&ThisClass::OnSetFocus>(WM_SETFOCUS),
        Route::ReflectNotify<&ThisClass::OnLvnItemChanged>(LVN_ITEMCHANGED),
        Route::Command<&CExtensionListControl::OnSearchExtension>(ID_EXTLIST_SEARCH_EXTENSION),
        Route::Command<&CExtensionListControl::OnExcludeExtension>(ID_FILTER_EXCLUDE_ITEM),
        Route::Window<&ThisClass::OnKeyDown>(WM_KEYDOWN),
    };
    return entries;
}
