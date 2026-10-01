// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#include "pch.h"

HBRUSH CDialog::OnCtlColor(CDC* pDC, WindowRef pWnd, const UINT nCtlColor)
{
    const HBRUSH brush = DarkMode::OnCtlColor(pDC, nCtlColor);
    return brush ? brush : CWnd::OnCtlColor(pDC, pWnd, nCtlColor);
}

std::optional<std::wstring> CDialog::PickFile(const FilePickerMode mode, std::wstring filter, WindowRef parent)
{
    std::ranges::replace(filter, L'|', L'\0');
    filter.push_back(L'\0');
    std::wstring file(4096, L'\0');
    OPENFILENAMEW dialog{ .lStructSize = sizeof(OPENFILENAMEW) };
    dialog.hwndOwner = GetDialogOwner(parent);
    dialog.lpstrFilter = filter.c_str();
    dialog.lpstrFile = file.data();
    dialog.nMaxFile = static_cast<DWORD>(file.size());
    dialog.lpstrDefExt = L"csv";
    dialog.Flags = OFN_EXPLORER | OFN_DONTADDTORECENT |
        (mode == FilePickerMode::Open ? OFN_PATHMUSTEXIST : OFN_OVERWRITEPROMPT);
    if (!(mode == FilePickerMode::Open ? GetOpenFileNameW(&dialog) : GetSaveFileNameW(&dialog)))
        return std::nullopt;
    return file.c_str();
}

std::optional<std::wstring> CDialog::PickFolder(WindowRef parent)
{
    auto folders = PickFolders(parent, false);
    return folders.empty() ? std::nullopt : std::optional(std::move(folders.front()));
}

std::vector<std::wstring> CDialog::PickFolders(WindowRef parent, const bool multiSelect)
{
    CComPtr<IFileOpenDialog> dialog;
    DWORD options = 0;
    if (FAILED(dialog.CoCreateInstance(CLSID_FileOpenDialog)) || FAILED(dialog->GetOptions(&options)) ||
        FAILED(dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_DONTADDTORECENT |
            (multiSelect ? FOS_ALLOWMULTISELECT : 0))) ||
        FAILED(dialog->SetTitle(L"WinDirStat")) || FAILED(dialog->Show(GetDialogOwner(parent)))) return {};

    CComPtr<IShellItemArray> results;
    DWORD count = 0;
    if (FAILED(dialog->GetResults(&results)) || FAILED(results->GetCount(&count))) return {};

    std::vector<std::wstring> folders;
    folders.reserve(count);
    for (DWORD i = 0; i < count; ++i)
    {
        CComPtr<IShellItem> result;
        CComHeapPtr<wchar_t> path;
        if (FAILED(results->GetItemAt(i, &result)) ||
            FAILED(result->GetDisplayName(SIGDN_FILESYSPATH, &path)) || path == nullptr) return {};
        folders.emplace_back(path);
    }
    return folders;
}

std::optional<COLORREF> CDialog::PickColor(const COLORREF initial, WindowRef parent)
{
    static COLORREF custom[16]{};
    CHOOSECOLORW dialog{ .lStructSize = sizeof(CHOOSECOLORW) };
    dialog.hwndOwner = GetDialogOwner(parent);
    dialog.rgbResult = initial;
    dialog.lpCustColors = custom;
    dialog.Flags = CC_FULLOPEN | CC_RGBINIT | CC_ANYCOLOR;
    if (!ChooseColorW(&dialog)) return std::nullopt;
    return dialog.rgbResult;
}

// -----------------------------------------------------------------------------
//  Cold toolbar and property-sheet setup
// -----------------------------------------------------------------------------
bool CPropertyPage::Create(WindowRef parent)
{
    if (m_hWnd != nullptr) return false;
    return CreateDialogParamW(GetAppInstance(), MAKEINTRESOURCEW(m_nIDTemplate), parent.m_hWnd,
        FrameworkDialogProc, reinterpret_cast<LPARAM>(static_cast<CWnd*>(this))) != nullptr;
}

void CPropertyPage::SetModified(const bool bChanged)
{
    m_bModified = bChanged;
    if (auto* sheet = GetParent<CPropertySheet>()) sheet->UpdateApplyButton();
}

bool CPropertySheet::SelectPage(const int i)
{
    if (i < 0 || i >= GetPageCount()) return false;
    if (m_tab.Handle() == nullptr)
    {
        m_pendingActivePage = i;
        return true;
    }

    if (!ActivatePage(i)) return false;
    SyncTabSelection(i);
    return true;
}

void CPropertySheet::UpdateApplyButton()
{
    if (IsWindow(m_applyButton))
        ::EnableWindow(m_applyButton, std::ranges::any_of(m_pages,
            [](const auto& page) { return page->m_bModified; }));
}

bool CPropertySheet::PreprocessMessage(MSG* pMsg)
{
    if (pMsg == nullptr || pMsg->message != WM_KEYDOWN) return CWnd::PreprocessMessage(pMsg);
    if (!IsKeyDown(VK_CONTROL) ||
        (pMsg->wParam != VK_TAB && pMsg->wParam != VK_PRIOR && pMsg->wParam != VK_NEXT))
        return CWnd::PreprocessMessage(pMsg);

    const int pageCount = GetPageCount();
    const int active = GetActivePageIndex();
    if (pageCount <= 0 || active < 0 || active >= pageCount) return CWnd::PreprocessMessage(pMsg);
    if (pageCount == 1) return true;

    const HWND activePage = m_pages[active]->Handle();
    const HWND focus = ::GetFocus();
    const bool moveFocus = focus != nullptr && activePage != nullptr &&
        (focus == activePage || ::IsChild(activePage, focus));
    const bool previous = pMsg->wParam == VK_PRIOR ||
        (pMsg->wParam == VK_TAB && IsKeyDown(VK_SHIFT));
    const int next = (active + pageCount + (previous ? -1 : 1)) % pageCount;
    if (!SelectPage(next)) return CWnd::PreprocessMessage(pMsg);

    if (moveFocus)
    {
        const HWND nextPage = m_pages[next]->Handle();
        const HWND firstChild = ::GetWindow(nextPage, GW_CHILD);
        const HWND lastTab = firstChild != nullptr ? GetNextDlgTabItem(nextPage, firstChild, true) : nullptr;
        const HWND firstTab = lastTab != nullptr ? GetNextDlgTabItem(nextPage, lastTab, false) : nullptr;
        const bool validTab = firstTab != nullptr && ::IsChild(nextPage, firstTab) && ::IsWindowVisible(firstTab) &&
            ::IsWindowEnabled(firstTab) && (GetWindowLongPtrW(firstTab, GWL_STYLE) & WS_TABSTOP) != 0;
        if (validTab)
            ::SendMessageW(nextPage, WM_NEXTDLGCTL, reinterpret_cast<WPARAM>(firstTab), true);
        else
            ::SetFocus(nextPage);
    }
    return true;
}

LRESULT CPropertySheet::OnRequestedPageChanged(const WPARAM wParam, const LPARAM)
{
    if (m_syncingTabSelection) return 0;
    const int requestedPage = static_cast<int>(wParam);
    if (const int previous = m_currentPage; !ActivatePage(requestedPage) && previous >= 0)
        SyncTabSelection(previous);
    return 0;
}

std::span<const RouteEntry> CPropertySheet::Routes()
{
    static constexpr std::array entries
    {
        Route::Window<&OnRequestedPageChanged>(WM_WDS_TAB_CHANGED),
        Route::Window<&OnCtlColor>(WM_CTLCOLOR),
        Route::Window<&OnClose>(WM_CLOSE),
    };
    return entries;
}

bool CPropertySheet::OnCommand(const WPARAM wParam, const LPARAM lParam)
{
    if (m_applying) return true;
    const UINT id = LOWORD(wParam);
    if (id == IDOK)
    {
        if (ApplyPages(id)) RequestModalExit(IDOK);
        return true;
    }
    if (id == IDCANCEL) { RequestModalExit(IDCANCEL); return true; }
    if (id == ID_APPLY_NOW)
    {
        ApplyPages(id);
        return true;
    }
    return CWnd::OnCommand(wParam, lParam);
}

bool CPropertySheet::EnsurePageCreated(const int i)
{
    if (i < 0 || i >= GetPageCount()) return false;

    CPropertyPage* page = m_pages[i].get();
    if (page->Handle() != nullptr) return true;

    if (!page->Create(this)) return false;
    page->ModifyStyle(WS_POPUP | WS_CAPTION | WS_THICKFRAME | WS_DISABLED, WS_CHILD, SWP_FRAMECHANGED);
    page->ModifyStyleEx(0, WS_EX_CONTROLPARENT);

    if (!m_pageRect.IsEmpty())
    {
        page->SetWindowPos(nullptr, m_pageRect.left, m_pageRect.top,
            m_pageRect.Width(), m_pageRect.Height(), SWP_NOZORDER | SWP_NOACTIVATE);
    }

    page->ShowWindow(SW_HIDE);
    return true;
}

bool CPropertySheet::ActivatePage(const int active)
{
    if (!EnsurePageCreated(active)) return false;
    if (m_currentPage == active) return true;

    for (const auto [i, page] : std::views::enumerate(m_pages))
        if (page->Handle())
        {
            const bool isActive = (static_cast<int>(i) == active);
            page->EnableWindow(isActive);
            page->ShowWindow(isActive ? SW_SHOW : SW_HIDE);
        }

    // Dialog traversal follows sibling Z-order, so the active page belongs immediately after the tab strip.
    m_pages[active]->SetWindowPos(&m_tab, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

    m_currentPage = active;
    return true;
}

void CPropertySheet::SyncTabSelection(const int active)
{
    if (m_tab.GetActiveTab() == active) return;
    m_syncingTabSelection = true;
    m_tab.SelectTab(active);
    m_syncingTabSelection = false;
}

bool CPropertySheet::ApplyPages(const UINT command)
{
    if (m_applying) return false;
    const ScopedValue applying(m_applying, true);
    for (const auto [index, page] : std::views::enumerate(m_pages))
    {
        if (page->Handle() == nullptr) continue;
        if (const auto error = page->PrepareApply())
        {
            SelectPage(static_cast<int>(index));
            ShowMessageBox(m_hWnd, error->message, m_caption, MB_OK | MB_ICONWARNING);
            ::SetFocus(error->control);
            return false;
        }
    }
    if (!ConfirmApply(command)) return false;
    for (const auto& page : m_pages) if (page->Handle() != nullptr) page->BeforeApply();
    for (const auto& page : m_pages) if (page->Handle() != nullptr) page->CommitApply();
    for (const auto& page : m_pages) page->m_bModified = false;
    UpdateApplyButton();
    for (const auto& page : m_pages) if (page->Handle() != nullptr) page->AfterApply();
    OnApplied(command);
    return true;
}

HBRUSH CPropertySheet::OnCtlColor(CDC* pDC, WindowRef pWnd, const UINT nCtlColor)
{
    const HBRUSH brush = DarkMode::OnCtlColor(pDC, nCtlColor);
    return brush ? brush : CWnd::OnCtlColor(pDC, pWnd, nCtlColor);
}

bool CPropertySheet::OnEraseBkgnd(CDC* pDC) const
{
    const CRect rc = GetClientRect();
    pDC->FillSolidRect(rc, DarkMode::SystemColor(COLOR_BTNFACE));
    return true;
}

bool CPropertySheet::OnInitDialog()
{
    SetText(m_caption);
    const int margin = ::ScaleForDpi(10, m_hWnd);
    const int tabH = ::ScaleForDpi(22, m_hWnd);
    const int tabGap = ::ScaleForDpi(4, m_hWnd);
    const int btnW = ::ScaleForDpi(86, m_hWnd);
    const int btnH = ::ScaleForDpi(26, m_hWnd);
    const int btnGap = ::ScaleForDpi(8, m_hWnd);
    const int gap = ::ScaleForDpi(8, m_hWnd);

    // Read tab labels from the dialog templates, then create only one page for
    // initial sizing.  Initializing every property page here makes the settings
    // dialog feel sluggish compared with MFC's lazy property-page creation.
    int maxW = 100, maxH = 100;
    std::vector<std::wstring> captions;
    captions.reserve(m_pages.size());
    for (const auto& page : m_pages)
    {
        captions.push_back([templateId = page->m_nIDTemplate]() -> std::wstring
            {
                const HINSTANCE instance = GetAppInstance();
                const HRSRC resource = FindResourceW(instance, MAKEINTRESOURCEW(templateId), RT_DIALOG);
                if (resource == nullptr) return {};

                const DWORD resourceSize = SizeofResource(instance, resource);
                const HGLOBAL global = LoadResource(instance, resource);
                const auto* begin = global != nullptr ? static_cast<const WORD*>(LockResource(global)) : nullptr;
                if (begin == nullptr || resourceSize < 2 * sizeof(WORD)) return {};

                const size_t wordCount = resourceSize / sizeof(WORD);
                const size_t headerWords = begin[0] == 1 && begin[1] == 0xFFFF ? 13 : 9;
                if (wordCount < headerWords) return {};

                const WORD* const end = begin + wordCount;
                const auto skipResourceOrString = [end](const WORD* value)
                    {
                        if (value >= end) return static_cast<const WORD*>(nullptr);
                        if (*value == 0) return value + 1;
                        if (*value == 0xFFFF) return end - value >= 2 ? value + 2 : nullptr;
                        while (value < end && *value != 0) ++value;
                        return value < end ? value + 1 : nullptr;
                    };

                const WORD* value = skipResourceOrString(begin + headerWords); // menu
                if (value == nullptr) return {};
                value = skipResourceOrString(value); // window class
                if (value == nullptr || value >= end || *value == 0 || *value == 0xFFFF) return {};

                const WORD* const start = value;
                while (value < end && *value != 0) ++value;
                if (value >= end) return {};
                return { reinterpret_cast<const wchar_t*>(start), static_cast<size_t>(value - start) };
            }());
    }

    for (const auto [i, page] : std::views::enumerate(m_pages))
    {
        if (!EnsurePageCreated(static_cast<int>(i))) continue;

        const CRect rcPage = page->GetWindowRect();
        maxW = std::max<int>(maxW, rcPage.Width());
        maxH = std::max<int>(maxH, rcPage.Height());
        break;
    }

    const int clientW = margin + maxW + margin;
    const int clientH = margin + tabH + tabGap + maxH + gap + btnH + margin;

    // Resize the sheet to fit
    RECT rcWin{ 0, 0, clientW, clientH };
    AdjustWindowRectEx(&rcWin, static_cast<DWORD>(GetStyle()), false,
        static_cast<DWORD>(GetWindowLongPtrW(m_hWnd, GWL_EXSTYLE)));
    SetWindowPos(nullptr, 0, 0, rcWin.right - rcWin.left, rcWin.bottom - rcWin.top, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);

    // Tab strip (label-only)
    m_tab.SetLocation(CTabControl::Location::Top);
    m_tab.Create(CRect(margin, margin, margin + maxW, margin + tabH), this, 0xCAFE, true);
    for (auto& cap : captions) m_tab.AddTab(nullptr, cap);

    // Position pages
    const int pageY = margin + tabH + tabGap;
    m_pageRect = CRect(margin, pageY, margin + maxW, pageY + maxH);
    for (const auto& page : m_pages)
        if (page->Handle()) page->SetWindowPos(nullptr, m_pageRect.left, m_pageRect.top,
            m_pageRect.Width(), m_pageRect.Height(), SWP_NOZORDER | SWP_NOACTIVATE);

    // Buttons (bottom-right: OK, Cancel, Apply)
    const int btnY = pageY + maxH + gap;
    int bx = margin + maxW - 3 * btnW - 2 * btnGap;
    const auto createButton = [this, btnY, btnW, btnH](const int x, const UINT id, const LPCWSTR text, const DWORD style)
        {
            const HWND button = CreateWindowExW(0, WC_BUTTONW, text, WS_CHILD | WS_VISIBLE | WS_TABSTOP | style,
                x, btnY, btnW, btnH, m_hWnd, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(id)),
                GetAppInstance(), nullptr);
            ::SendMessageW(button, WM_SETFONT, reinterpret_cast<WPARAM>(GetAppFont(button)), true);
            return button;
        };
    createButton(bx, IDOK, L"IDS_GENERIC_OK", BS_DEFPUSHBUTTON);
    bx += btnW + btnGap;
    createButton(bx, IDCANCEL, L"IDS_GENERIC_CANCEL", BS_PUSHBUTTON);
    bx += btnW + btnGap;
    m_applyButton = createButton(bx, ID_APPLY_NOW, L"IDS_GENERIC_APPLY", BS_PUSHBUTTON);

    if (GetPageCount() > 0) SelectPage(std::clamp(m_pendingActivePage, 0, GetPageCount() - 1));
    UpdateApplyButton();
    CenterWindow();
    return true;
}

INT_PTR CPropertySheet::ShowModal()
{
    const struct
    {
        DLGTEMPLATE dialog;
        WORD menu, windowClass, title;
    } layout{{ WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN | DS_MODALFRAME | DS_CENTER,
        WS_EX_CONTROLPARENT, 0, 0, 0, 400, 300 }, 0, 0, 0};
    return ShowModalTemplate(&layout.dialog);
}
