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

#include "pch.h"
#include "Filtering.h"
#include "TreeMapView.h"
#include "FileTabbedView.h"
#include "FileTreeView.h"
#include "DrawTextCache.h"
#include "ExtensionView.h"
#include "PageAdvanced.h"
#include "PageFiltering.h"
#include "PageCleanups.h"
#include "PageFileTree.h"
#include "PageTreeMap.h"
#include "PagePermissions.h"
#include "PageGeneral.h"
#include "PagePrompts.h"
#include "ProgressDlg.h"

// Clipboard Opener
class COpenClipboard final
{
    BOOL m_open = FALSE;
    BOOL m_ready = FALSE;

public:
    COpenClipboard(CWnd* owner) noexcept
    {
        m_open = owner->OpenClipboard();
        if (m_open)
        {
            m_ready = EmptyClipboard();
        }
    }

    bool IsReady() const noexcept { return m_ready; }

    ~COpenClipboard() noexcept
    {
        if (m_open)
        {
            CloseClipboard();
        }
    }
};

/////////////////////////////////////////////////////////////////////////////

IMPLEMENT_DYNAMIC(COptionsPropertySheet, CMFCPropertySheet)

COptionsPropertySheet::COptionsPropertySheet()
    : CMFCPropertySheet(Localization::Lookup(IDS_WINDIRSTAT_SETTINGS).c_str())
{
    m_look = PropSheetLook_OneNoteTabs;
}

void COptionsPropertySheet::SetRestartRequired(const bool changed)
{
    m_restartRequest = changed;
}

BEGIN_MESSAGE_MAP(COptionsPropertySheet, CMFCPropertySheet)
    ON_WM_CTLCOLOR()
    ON_WM_ERASEBKGND()
END_MESSAGE_MAP()

BOOL COptionsPropertySheet::OnEraseBkgnd(CDC* pDC)
{
    if (!DarkMode::IsDarkModeActive())
    {
        return CMFCPropertySheet::OnEraseBkgnd(pDC);
    }

    // Paint the background with dark mode color
    const CRect rect = ClientRectOf(this);
    pDC->FillSolidRect(&rect, DarkMode::WdsSysColor(CTLCOLOR_DLG));

    return TRUE;
}

HBRUSH COptionsPropertySheet::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
    const HBRUSH brush = DarkMode::OnCtlColor(pDC, nCtlColor);
    return brush ? brush : CMFCPropertySheet::OnCtlColor(pDC, pWnd, nCtlColor);
}

BOOL COptionsPropertySheet::OnInitDialog()
{
    const BOOL bResult = CMFCPropertySheet::OnInitDialog();
    CTabCtrlHelper::SetupTabControl(GetTab(), CMFCTabCtrl::STYLE_FLAT);

    Localization::UpdateDialogs(*this);
    Localization::UpdateTabControl(GetTab());
    DarkMode::AdjustControls(GetSafeHwnd());

    const int page = (m_initialPage >= 0) ? m_initialPage : static_cast<int>(COptions::ConfigPage);
    SetActivePage(std::min((int)page, (int)GetPageCount() - 1));
    return bResult;
}

BOOL COptionsPropertySheet::PreTranslateMessage(MSG* pMsg)
{
    if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_ESCAPE)
    {
        EndDialog(IDCANCEL);
        return TRUE;
    }
    return CMFCPropertySheet::PreTranslateMessage(pMsg);
}

bool COptionsPropertySheet::ShowSettings(const int initialPage)
{
    auto sheet = std::make_unique<COptionsPropertySheet>();
    sheet->m_initialPage = initialPage; // -1 means restore last-used tab

    auto general = std::make_unique<CPageGeneral>();
    auto filtering = std::make_unique<CPageFiltering>();
    auto treelist = std::make_unique<CPageFileTree>();
    auto treemap = std::make_unique<CPageTreeMap>();
    auto permissions = std::make_unique<CPagePermissions>();
    auto cleanups = std::make_unique<CPageCleanups>();
    auto prompts = std::make_unique<CPagePrompts>();
    auto advanced = std::make_unique<CPageAdvanced>();

    sheet->AddPage(general.get());
    sheet->AddPage(filtering.get()); // index 1
    sheet->AddPage(treelist.get());
    sheet->AddPage(treemap.get());
    sheet->AddPage(permissions.get());
    sheet->AddPage(cleanups.get());
    sheet->AddPage(prompts.get());
    sheet->AddPage(advanced.get());

    sheet->DoModal();
    return sheet->m_restartApplication;
}

BOOL COptionsPropertySheet::OnCommand(const WPARAM wParam, const LPARAM lParam)
{
    COptions::ConfigPage = GetActiveIndex();

    if (const int cmd = LOWORD(wParam); IDOK == cmd || ID_APPLY_NOW == cmd)
    {
        if (m_restartRequest && (IDOK == cmd || !m_alreadyAsked))
        {
            const int r = WdsMessageBox(*this, Localization::Lookup(IDS_RESTART_REQUEST),
                wds::strWinDirStat, MB_YESNOCANCEL);
            if (IDCANCEL == r)
            {
                return true; // "Message handled". Don't proceed.
            }
            else if (IDNO == r)
            {
                m_alreadyAsked = true; // Don't ask twice.
            }
            else
            {
                ASSERT(IDYES == r);
                m_restartApplication = true;

                if (ID_APPLY_NOW == cmd)
                {
                    // This _posts_ a message...
                    EndDialog(IDOK);
                    // ... so after returning from this function, the OnOK()-handlers
                    // of the pages will be called, before the sheet is closed.
                }
            }
        }
    }

    return CMFCPropertySheet::OnCommand(wParam, lParam);
}

/////////////////////////////////////////////////////////////////////////////

CWdsSplitterWnd::CWdsSplitterWnd(double* splitterPos) :
    m_userSplitterPos(splitterPos)
{
    m_wasTrackedByUser = (*splitterPos > 0 && *splitterPos < 1);
}

void CWdsSplitterWnd::PostNcDestroy()
{
    // VS 2022 MFC no longer resets row/col state in PostNcDestroy, causing ASSERT in CreateStatic on next call.
    delete[] m_pRowInfo;  m_pRowInfo  = nullptr;
    delete[] m_pColInfo;  m_pColInfo  = nullptr;
    m_nRows = m_nMaxRows = 0;
    m_nCols = m_nMaxCols = 0;
    CSplitterWndEx::PostNcDestroy();
}

BEGIN_MESSAGE_MAP(CWdsSplitterWnd, CSplitterWndEx)
    ON_WM_SIZE()
END_MESSAGE_MAP()

void CWdsSplitterWnd::StopTracking(const BOOL bAccept)
{
    CSplitterWndEx::StopTracking(bAccept);
    if (!bAccept) return;

    int currentPos = 0, dummy = 0;
    const bool isVertical = (GetColumnCount() > 1);
    isVertical ? GetColumnInfo(0, currentPos, dummy) : GetRowInfo(0, currentPos, dummy);

    const CRect rcClient  = ClientRectOf(this);
    const int   totalSize = isVertical ? rcClient.Width() : rcClient.Height();
    if (totalSize <= 0) return;

    const int paneSize[2] = { currentPos, totalSize - currentPos };
    for (int pane = 0; pane < 2; ++pane)
    {
        const PaneTracking& tracking = m_paneTracking[pane];
        if (!tracking.onToggle) continue;

        const bool isVisible = paneSize[pane] > DpiRest(COptions::MinimizeViewThreshold);
        tracking.onToggle(isVisible);

        if (!isVisible)
        {
            if (tracking.onMinimize) tracking.onMinimize();
            return;
        }
    }

    m_splitterPos       = static_cast<double>(currentPos) / totalSize;
    m_wasTrackedByUser  = true;
    *m_userSplitterPos  = m_splitterPos;
}

void CWdsSplitterWnd::ClearPaneTracking()
{
    m_paneTracking[0] = {};
    m_paneTracking[1] = {};
}

void CWdsSplitterWnd::TrackPane(const int pane, std::function<void(bool)> onToggle, std::function<void()> onMinimize)
{
    ASSERT(pane == 0 || pane == 1);
    if (pane == 0 || pane == 1)
        m_paneTracking[pane] = { std::move(onToggle), std::move(onMinimize) };
}

void CWdsSplitterWnd::SetSplitterPos(const double pos)
{
    m_splitterPos = pos;
    const CRect rc = ClientRectOf(this);
    if (GetColumnCount() > 1)
    {
        if (const int cx = static_cast<int>(pos * rc.Width()); m_pColInfo && cx >= 0)
            { SetColumnInfo(0, cx, 0); RecalcLayout(); }
    }
    else
    {
        if (const int cy = static_cast<int>(pos * rc.Height()); m_pRowInfo && cy >= 0)
            { SetRowInfo(0, cy, 0); RecalcLayout(); }
    }
}

void CWdsSplitterWnd::RestoreSplitterPos(const double posIfVirgin)
{
    SetSplitterPos(m_wasTrackedByUser ? *m_userSplitterPos : posIfVirgin);
}

void CWdsSplitterWnd::OnSize(const UINT nType, const int cx, const int cy)
{
    if (GetColumnCount() > 1)
    {
        if (const int v = static_cast<int>(cx * m_splitterPos); v > 0) SetColumnInfo(0, v, 0);
    }
    else
    {
        if (const int v = static_cast<int>(cy * m_splitterPos); v > 0) SetRowInfo(0, v, 0);
    }
    CSplitterWndEx::OnSize(nType, cx, cy);
}

/////////////////////////////////////////////////////////////////////////////

void CPacmanControl::Drive()
{
    if (IsWindow(m_hWnd))
    {
        m_pacman.UpdatePosition();
        RedrawWindow();
    }
}

void CPacmanControl::Start()
{
    m_pacman.Start();
}

void CPacmanControl::Stop()
{
    m_pacman.Stop();
}

BEGIN_MESSAGE_MAP(CPacmanControl, CWnd)
    ON_WM_PAINT()
    ON_WM_CREATE()
    ON_WM_ERASEBKGND()
END_MESSAGE_MAP()

int CPacmanControl::OnCreate(const LPCREATESTRUCT lpCreateStruct)
{
    if (CWnd::OnCreate(lpCreateStruct) == -1)
    {
        return -1;
    }

    m_pacman.Reset();
    m_pacman.Start();
    return 0;
}

BOOL CPacmanControl::OnEraseBkgnd(CDC* pDC)
{
    UNREFERENCED_PARAMETER(pDC);
    return TRUE;
}

void CPacmanControl::OnPaint()
{
    CPaintDC dc(this);
    CMemDC memDC(dc, this);
    CDC* pDC = &memDC.GetDC();

    // Draw the animation
    const CRect rc = ClientRectOf(this);
    m_pacman.Draw(pDC, rc, DarkMode::WdsSysColor(
        DarkMode::IsDarkModeActive() ? COLOR_WINDOW : COLOR_BTNFACE));

    // Draw the borders
    CMFCVisualManager::GetInstance()->OnDrawStatusBarPaneBorder(
        pDC, &CMainFrame::Get()->m_wndStatusBar, rc, 0, CMainFrame::Get()->GetStyle());
}

/////////////////////////////////////////////////////////////////////////////

void CDeadFocusWnd::Create(CWnd* parent)
{
    const CRect rc(0, 0, 0, 0);
    CWnd::Create(AfxRegisterWndClass(0, nullptr, nullptr, nullptr), L"_deadfocus", WS_CHILD, rc, parent, 0);
}

CDeadFocusWnd::~CDeadFocusWnd()
{
    CWnd::DestroyWindow();
}

BEGIN_MESSAGE_MAP(CDeadFocusWnd, CWnd)
    ON_WM_KEYDOWN()
END_MESSAGE_MAP()

void CDeadFocusWnd::OnKeyDown(const UINT nChar, UINT /*nRepCnt*/, UINT /*nFlags*/ )
{
    if (nChar == VK_TAB)
    {
        CMainFrame::Get()->MoveFocus(LF_FILETREE);
    }
}

/////////////////////////////////////////////////////////////////////////////
UINT CMainFrame::s_TaskBarMessage = ::RegisterWindowMessage(L"TaskbarButtonCreated");

IMPLEMENT_DYNCREATE(CMainFrame, CFrameWndEx)

BEGIN_MESSAGE_MAP(CMainFrame, CFrameWndEx)
    ON_COMMAND(ID_CONFIGURE, OnConfigure)
    ON_COMMAND(ID_VIEW_SHOWFILETYPES, OnViewShowFileTypes)
    ON_COMMAND(ID_VIEW_GROUP_TYPES, OnViewGroupUnregisteredTypes)
    ON_COMMAND(ID_VIEW_SHOWTREEMAP, OnViewShowTreeMap)
    ON_COMMAND(ID_TREEMAP_LOGICAL_SIZE, OnViewTreeMapUseLogical)
    ON_MESSAGE(WM_ENTERSIZEMOVE, OnEnterSizeMove)
    ON_MESSAGE(WM_EXITSIZEMOVE, OnExitSizeMove)
    ON_MESSAGE(WM_CALLBACKUI, OnCallbackRequest)
    ON_MESSAGE(DarkMode::WM_UAHDRAWMENU, OnUahDrawMenu)
    ON_MESSAGE(DarkMode::WM_UAHDRAWMENUITEM, OnUahDrawMenu)
    ON_REGISTERED_MESSAGE(s_TaskBarMessage, OnTaskButtonCreated)
    ON_UPDATE_COMMAND_UI(ID_VIEW_SHOWTREEMAP, OnUpdateViewShowTreeMap)
    ON_UPDATE_COMMAND_UI(ID_VIEW_SHOWFILETYPES, OnUpdateViewShowFileTypes)
    ON_UPDATE_COMMAND_UI(ID_VIEW_GROUP_TYPES, OnUpdateViewGroupUnregisteredTypes)
    ON_UPDATE_COMMAND_UI(ID_TREEMAP_LOGICAL_SIZE, OnUpdateTreeMapUseLogical)
    ON_COMMAND(ID_TREEMAP_SHOW_EXTENSIONS, OnViewShowExtensionsOnTreeMap)
    ON_UPDATE_COMMAND_UI(ID_TREEMAP_SHOW_EXTENSIONS, OnUpdateViewShowExtensionsOnTreeMap)
    ON_COMMAND(ID_TREEMAP_SHOW_FOLDER_FRAMES, OnViewShowFolderFramesOnTreeMap)
    ON_UPDATE_COMMAND_UI(ID_TREEMAP_SHOW_FOLDER_FRAMES, OnUpdateViewShowFolderFramesOnTreeMap)
    ON_UPDATE_COMMAND_UI(ID_TOOLS_WATCHER, OnUpdateViewShowWatcher)
    ON_WM_CLOSE()
    ON_WM_CREATE()
    ON_WM_DESTROY()
    ON_WM_INITMENUPOPUP()
    ON_WM_SIZE()
    ON_WM_SYSCOLORCHANGE()
    ON_WM_POWERBROADCAST()
    ON_WM_TIMER()
    ON_WM_NCPAINT()
    ON_WM_NCACTIVATE()
    ON_COMMAND(ID_VIEW_ALL_FILES, &CMainFrame::OnViewAllFiles)
    ON_COMMAND(ID_VIEW_LARGEST_FILES, &CMainFrame::OnViewLargestFiles)
    ON_COMMAND(ID_VIEW_DUPLICATE_FILES, &CMainFrame::OnViewDuplicateFiles)
    ON_COMMAND(ID_VIEW_SEARCH_RESULTS, &CMainFrame::OnViewSearchResults)
    ON_COMMAND(ID_VIEW_LARGE_TOOLBAR, &CMainFrame::OnViewLargeToolBar)
    ON_UPDATE_COMMAND_UI(ID_VIEW_LARGE_TOOLBAR, &CMainFrame::OnUpdateViewLargeToolBar)
    ON_COMMAND_RANGE(ID_TOOLS_SHADOW_COPY_BASE, ID_TOOLS_SHADOW_COPY_BASE + wds::alphaSize, &CMainFrame::OnAdvancedShadowCopy)
    ON_COMMAND_RANGE(ID_TOOLS_DEFRAG_BASE, ID_TOOLS_DEFRAG_BASE + wds::alphaSize, &CMainFrame::OnAdvancedDefrag)
    ON_COMMAND_RANGE(ID_TOOLS_CHKDSK_BASE, ID_TOOLS_CHKDSK_BASE + wds::alphaSize, &CMainFrame::OnAdvancedChkdsk)
    ON_COMMAND(ID_TOOLS_WATCHER, &CMainFrame::OnToolsWatcher)
    ON_COMMAND(ID_TOOLS_PERMISSIONS, &CMainFrame::OnToolsPermissions)
    ON_UPDATE_COMMAND_UI(ID_TOOLS_PERMISSIONS, OnUpdateToolsPermissions)
    ON_COMMAND(ID_TOOLS_STORAGE_ANALYTICS, &CMainFrame::OnToolsStorageAnalytics)
    ON_UPDATE_COMMAND_UI(ID_TOOLS_STORAGE_ANALYTICS, &CMainFrame::OnUpdateToolsStorageAnalytics)
    ON_COMMAND(ID_VIEW_WINDOW_LAYOUT, &CMainFrame::OnViewWindowLayout)
END_MESSAGE_MAP()

constexpr auto ID_STATUSPANE_IDLE_INDEX = 0;
constexpr auto ID_STATUSPANE_SIZE_INDEX = 1;
constexpr auto ID_STATUSPANE_RAM_INDEX = 2;

CMainFrame::CMainFrame()
{
    s_Singleton = this;
}

CMainFrame::~CMainFrame()
{
    s_Singleton = nullptr;
}

LRESULT CMainFrame::OnTaskButtonCreated(WPARAM, LPARAM)
{
    if (!m_taskbarList)
    {
        if (FAILED(CoCreateInstance(CLSID_TaskbarList, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&m_taskbarList))))
        {
            VTRACE(L"CoCreateInstance(CLSCID_TaskbarList, nullptr, CLSCTX_ALL) failed");
        }
    }
    return 0;
}

void CMainFrame::CreateProgress(ULONGLONG range)
{
    // Directory structure may contain other volume or internal loops
    // so set range to indicate there is no range so display pacman
    if (!COptions::ExcludeVolumeMountPoints ||
        !COptions::ExcludeJunctions ||
        !COptions::ExcludeSymbolicLinksDirectory)
    {
        range = 0;
    }

    m_progressRange = range;
    m_progressPos = 0;
    m_progressVisible = true;
    if (range > 0)
    {
        CreateStatusProgress();
    }
    else
    {
        CreatePacmanProgress();
    }
}

void CMainFrame::UpdateProgressRange(const ULONGLONG range)
{
    // Resync the range after hardlink adjustment so that the scan-based position
    // and the range share the same calculation basis, converging exactly at 100%.
    if (m_progressVisible && m_progressRange > 0)
        m_progressRange = range;
}

void CMainFrame::SetProgressComplete()
{
    // Disable any potential suspend state
    SuspendState(false);

    if (m_taskbarList)
    {
        m_taskbarList->SetProgressState(*this, m_taskbarButtonState = TBPF_NOPROGRESS);
    }

    DestroyProgress();
    CDirStatDoc::Get()->SetTitlePrefix(wds::strEmpty);
    CFileTreeControl::Get()->SortItems();
    CFileDupeControl::Get()->SortItems();
    CFileTopControl::Get()->SortItems();
}

bool CMainFrame::IsScanSuspended() const
{
    return m_scanSuspend;
}

void CMainFrame::SuspendState(const bool suspend)
{
    m_scanSuspend = suspend;
    if (m_taskbarList)
    {
        if (suspend && m_taskbarButtonState != TBPF_PAUSED)
        {
            m_taskbarButtonPreviousState = m_taskbarButtonState;
            m_taskbarList->SetProgressState(*this, m_taskbarButtonState = TBPF_PAUSED);
        }
        else if (!suspend && m_taskbarButtonState == TBPF_PAUSED)
        {
            m_taskbarList->SetProgressState(*this, m_taskbarButtonState = m_taskbarButtonPreviousState);
        }
    }
    CPacman::SetGlobalSuspendState(suspend);
    UpdateProgress();
}

void CMainFrame::UpdateProgress()
{
    // Update working item tracker if changed
    const auto currentRoot = CDirStatDoc::Get()->GetRootItem();
    if (currentRoot != m_workingItem &&
        currentRoot != nullptr && !currentRoot->IsDone())
    {
        m_workingItem = currentRoot;
        CreateProgress(m_workingItem->GetProgressRange());
    }

    // Exit early if we are not ready for visual updates
    if (!m_progressVisible || m_workingItem == nullptr || currentRoot == nullptr) return;

    // Update pacman graphic (does nothing if hidden)
    m_progressPos = m_workingItem->GetProgressPos();
    m_pacman.Drive();

    std::wstring titlePrefix;
    std::wstring suspended;

    // Display the suspend text in the bar if suspended
    if (IsScanSuspended())
    {
        static const std::wstring suspendString = Localization::Lookup(IDS_SUSPENDED);
        suspended = suspendString;
    }

    if (m_progressRange > 0 && m_progress.m_hWnd != nullptr)
    {
        // Limit progress at 100% as hard-linked files will count twice
        const int pos = std::min(static_cast<int>((m_progressPos * 100ull) / m_progressRange), 100);
        m_progress.SetPos(pos);

        titlePrefix = std::to_wstring(pos) + L"% " + suspended;
        if (m_taskbarList && m_taskbarButtonState != TBPF_PAUSED)
        {
            if (pos == 100)
            {
                m_taskbarList->SetProgressState(*this, m_taskbarButtonState = TBPF_INDETERMINATE);
            }
            else
            {
                m_taskbarList->SetProgressState(*this, m_taskbarButtonState = TBPF_NORMAL);
                m_taskbarList->SetProgressValue(*this, m_progressPos, m_progressRange);
            }
        }
    }
    else
    {
        static const std::wstring scanningString = Localization::Lookup(IDS_SCANNING);
        titlePrefix = scanningString + L" " + suspended;
    }

    TrimString(titlePrefix);
    CDirStatDoc::Get()->SetTitlePrefix(titlePrefix);
}

void CMainFrame::CreateStatusProgress()
{
    UpdatePaneText();
    if (m_progress.m_hWnd == nullptr)
    {
        CRect rc;
        m_wndStatusBar.GetItemRect(ID_STATUSPANE_IDLE_INDEX, rc);
        rc.DeflateRect(DpiRest(3, &m_wndStatusBar), DpiRest(4, &m_wndStatusBar),
            DpiRest(5, &m_wndStatusBar), DpiRest(4, &m_wndStatusBar));
        m_progress.Create(WS_CHILD | WS_VISIBLE, rc, &m_wndStatusBar, ID_WDS_CONTROL);
        m_progress.ModifyStyle(WS_BORDER, 0);

        if (DarkMode::IsDarkModeActive())
        {
            // Disable theming for progress bar to avoid light background in dark mode
            SetWindowTheme(m_progress.GetSafeHwnd(), L"", L"");
            m_progress.SetBkColor(DarkMode::WdsSysColor(COLOR_WINDOWFRAME));
            m_progress.ModifyStyleEx(WS_EX_STATICEDGE, 0);
        }
    }
    if (m_taskbarList)
    {
        m_taskbarList->SetProgressState(*this, m_taskbarButtonState = TBPF_INDETERMINATE);
    }
}

void CMainFrame::CreatePacmanProgress()
{
    if (m_pacman.m_hWnd == nullptr)
    {
        // Get rectangle and remove top/bottom border dimension
        CRect rc;
        m_wndStatusBar.GetItemRect(0, rc);
        m_pacman.Create(nullptr, nullptr, WS_CHILD | WS_VISIBLE, rc, &m_wndStatusBar, ID_WDS_CONTROL);
        m_pacman.Start();
    }
}

void CMainFrame::DestroyProgress()
{
    if (IsWindow(m_progress.m_hWnd))
    {
        m_progress.DestroyWindow();
        m_progress.m_hWnd = nullptr;
    }
    else if (IsWindow(m_pacman.m_hWnd))
    {
        m_pacman.Stop();
        m_pacman.DestroyWindow();
        m_pacman.m_hWnd = nullptr;
    }

    m_workingItem = nullptr;
    m_progressVisible = false;
    UpdatePaneText();
}

void CMainFrame::SetStatusPaneText(const CDC& cdc, const int pos,
    const std::wstring & text, const int minWidth)
{
    // do not process the update if text is the same
    static std::unordered_map<int, std::wstring> last;
    if (const auto it = last.find(pos); it != last.end() && it->second == text) return;
    last.insert_or_assign(pos, text);

    // set status path width and then set text
    const auto cx = cdc.GetTextExtent(text.c_str(), static_cast<int>(text.size())).cx;
    m_wndStatusBar.SetPaneWidth(pos, std::max((int)cx, DpiRest(minWidth)));
    m_wndStatusBar.SetPaneText(pos, text.c_str());
}

int CMainFrame::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
    if (CFrameWndEx::OnCreate(lpCreateStruct) == -1)
    {
        return -1;
    }

    // Setup status pane and force initial field population
    m_wndStatusBar.Create(this);
    constexpr UINT indicators[]{ ID_INDICATOR_IDLE ,ID_INDICATOR_SIZE, ID_INDICATOR_RAM };
    m_wndStatusBar.SetIndicators(indicators, _countof(indicators));
    m_wndStatusBar.SetPaneStyle(ID_STATUSPANE_IDLE_INDEX, SBPS_STRETCH);
    UpdatePaneText();

    // Setup status pane for dark mode
    if (DarkMode::IsDarkModeActive())
    {
        for (const int i : std::views::iota(0, m_wndStatusBar.GetCount()))
        {
            m_wndStatusBar.SetPaneBackgroundColor(i, DarkMode::WdsSysColor(COLOR_WINDOW));
        }
    }

    m_wndToolBar.CreateEx(this, TBSTYLE_FLAT, WS_CHILD | WS_VISIBLE | CBRS_TOP | CBRS_TOOLTIPS | CBRS_SIZE_DYNAMIC);
    m_wndToolBar.SetBorders(CRect());
    m_wndToolBar.SetPaneStyle(m_wndToolBar.GetPaneStyle() & ~CBRS_GRIPPER);
    m_wndToolBar.SetHeight(m_wndToolBar.GetRowHeight());
    DockPane(&m_wndToolBar);

    // Save the default button size (DPI-scaled) before any SetSizes call
    const auto initialButtonSize = m_wndToolBar.GetButtonSize();
    m_defaultButtonSize = { DpiRest(initialButtonSize.cx), DpiRest(initialButtonSize.cy) };
    RebuildToolBar();

    // Show or hide status bar if requested
    if (!COptions::ShowStatusBar) m_wndStatusBar.ShowWindow(SW_HIDE);
    if (!COptions::ShowToolBar) m_wndToolBar.ShowWindow(SW_HIDE);
    m_wndDeadFocus.Create(this);

    // setup look and feel with dark mode support
    CMFCVisualManager::SetDefaultManager(DarkMode::IsDarkModeActive() ?
        RUNTIME_CLASS(CDarkModeVisualManager) : RUNTIME_CLASS(CMFCVisualManagerWindows));

    // apply dark mode to main frame window
    DarkMode::AdjustControls(GetSafeHwnd());

    return 0;
}

void CMainFrame::InitialShowWindow()
{
    const WINDOWPLACEMENT wpsetting = COptions::MainWindowPlacement;
    if (wpsetting.length != 0)
    {
        SetWindowPlacement(&wpsetting);
    }

    SetTimer(ID_WDS_CONTROL, 25, nullptr);
}

void CMainFrame::InvokeInMessageThread(std::function<void()> callback) const
{
    if (CDirStatApp::Get()->m_nThreadID == GetCurrentThreadId()) callback();
    else Get()->SendMessage(WM_CALLBACKUI, 0, reinterpret_cast<LPARAM>(&callback));
}

void CMainFrame::OnClose()
{
    CWaitCursor wc;

    // Mark process as shutting down
    m_shuttingDown = true;

    // Suspend the scan and wait for scan to complete
    CDirStatDoc::Get()->StopScanningEngine(CDirStatDoc::Abort);

    // Stop icon queue
    GetIconHandler()->StopAsyncShellInfoQueue();

    // It's too late, to do this in OnDestroy(). Because the toolbar, if undocked,
    // is already destroyed in OnDestroy(). So we must save the toolbar state here
    // in OnClose().
    COptions::ShowToolBar = (m_wndToolBar.GetStyle() & WS_VISIBLE) != 0;
    COptions::ShowStatusBar = (m_wndStatusBar.GetStyle() & WS_VISIBLE) != 0;

    CFrameWndEx::OnClose();
}

void CMainFrame::OnDestroy()
{
    // Mark process as shutting down
    m_shuttingDown = true;

    // Force early cleanup of taskbar resources
    m_taskbarList.Release();

    // Save our window position
    WINDOWPLACEMENT wp = { .length = sizeof(wp) };
    GetWindowPlacement(&wp);
    COptions::MainWindowPlacement = wp;

    COptions::ShowFileTypes = GetExtensionView()->IsShowTypes();
    COptions::ShowTreeMap = GetTreeMapView()->IsShowTreeMap();

    // Close all artifacts and our child windows
    CFrameWndEx::OnDestroy();

    // Persist values at very end after all children have closed
    PersistedSetting::WritePersistedProperties();
}

BOOL CMainFrame::OnCreateClient(LPCREATESTRUCT /*lpcs*/, CCreateContext* pContext)
{
    m_splitter.CreateStatic(this, 2, 1);
    m_splitter.CreateView(1, 0, RUNTIME_CLASS(CTreeMapView), CSize(100, 100), pContext);
    m_subSplitter.CreateStatic(&m_splitter, 1, 2, WS_CHILD | WS_VISIBLE | WS_BORDER, m_splitter.IdFromRowCol(0, 0));
    m_subSplitter.CreateView(0, 0, RUNTIME_CLASS(CFileTabbedView), CSize(700, 500), pContext);
    m_subSplitter.CreateView(0, 1, RUNTIME_CLASS(CExtensionView), CSize(100, 500), pContext);

    m_treeMapView    = DYNAMIC_DOWNCAST(CTreeMapView,    m_splitter.GetPane(1, 0));
    m_fileTabbedView = DYNAMIC_DOWNCAST(CFileTabbedView, m_subSplitter.GetPane(0, 0));
    m_extensionView  = DYNAMIC_DOWNCAST(CExtensionView,  m_subSplitter.GetPane(0, 1));

    GetExtensionView()->ShowTypes(COptions::ShowFileTypes);
    GetTreeMapView()->ShowTreeMap(COptions::ShowTreeMap);

    m_layoutPopup.Create(this);
    RebuildLayout();
    return TRUE;
}

BOOL CMainFrame::PreCreateWindow(CREATESTRUCT& cs)
{
    // seed initial Title bar text
    static std::wstring title = Localization::LookupNeutral(AFX_IDS_APP_TITLE) + (IsElevationActive() ? std::format(L" ({})", Localization::Lookup(IDS_ADMIN)) : L"");
    cs.style &= ~FWS_ADDTOTITLE;
    cs.lpszName = title.c_str();

    if (!CFrameWndEx::PreCreateWindow(cs))
    {
        return FALSE;
    }

    // Prevent flashing of the main window when launching in non-interactive mode
    if (!CDirStatApp::Get()->GetSaveToPath().empty() ||
        !CDirStatApp::Get()->GetSaveDupesToPath().empty() ||
        !CDirStatApp::Get()->GetSavePermsToPath().empty())
    {
        AfxGetApp()->m_nCmdShow = SW_HIDE;
        cs.style &= ~WS_VISIBLE;
        cs.dwExStyle |= WS_EX_NOACTIVATE;
    }

    return TRUE;
}

void CMainFrame::MinimizeExtensionView()
{
    const int topo = COptions::LayoutTopology;
    const int perm = COptions::LayoutPermutation;

    // LT_COLS_THREE perm 2/3: ExtV is in m_subSplitter col 0 (first pane)
    if (topo == LT_COLS_THREE && (perm == 2 || perm == 3))
        m_subSplitter.SetSplitterPos(0.0);
    // LT_COLS_THREE perm 0/1 and LT_COLS_SUB_ROWS: ExtV is in m_splitter col 1
    else if (topo == LT_COLS_THREE || topo == LT_COLS_SUB_ROWS)
        m_splitter.SetSplitterPos(1.0);
    // LT_ROWS_SUB_COLS: ExtV is in m_subSplitter col 1
    else
        m_subSplitter.SetSplitterPos(1.0);
}

void CMainFrame::RestoreExtensionView()
{
    if (!GetExtensionView()->IsShowTypes()) return;

    const int topo = COptions::LayoutTopology;
    const int perm = COptions::LayoutPermutation;

    if (topo == LT_COLS_THREE && (perm == 2 || perm == 3))
        m_subSplitter.RestoreSplitterPos(1.0 / 3.0);
    else if (topo == LT_COLS_SUB_ROWS)
        m_splitter.RestoreSplitterPos(0.75);
    else if (topo == LT_COLS_THREE)
        m_splitter.RestoreSplitterPos(0.80);
    else
        m_subSplitter.RestoreSplitterPos(0.75);  // LT_ROWS_SUB_COLS

    GetExtensionView()->RedrawWindow();
}

void CMainFrame::ExpandFileTabbedView()
{
    const int topo = COptions::LayoutTopology;
    const int perm = COptions::LayoutPermutation;

    // Collapse whichever main-splitter pane doesn't contain FTV.
    const bool ftvInPane1 = (topo == LT_ROWS_SUB_COLS && perm == 1) ||
                             (topo == LT_COLS_THREE   && perm == 3);
    m_splitter.SetSplitterPos(ftvInPane1 ? 0.0 : 1.0);

    // LT_COLS_THREE perm 2: FTV is directly in main splitter, no sub-splitter needed.
    if (topo == LT_COLS_THREE && perm == 2)
        return;

    // Collapse whichever sub-splitter pane doesn't contain FTV.
    const bool ftvInSubPane1 = (topo == LT_COLS_THREE    && (perm == 1 || perm == 3)) ||
                                (topo == LT_COLS_SUB_ROWS && perm == 0);
    m_subSplitter.SetSplitterPos(ftvInSubPane1 ? 0.0 : 1.0);
}

void CMainFrame::MinimizeTreeMapView()
{
    const int topo = COptions::LayoutTopology;
    const int perm = COptions::LayoutPermutation;

    if (topo == LT_ROWS_SUB_COLS)
        m_splitter.SetSplitterPos(perm == 0 ? 1.0 : 0.0);
    else if (topo == LT_COLS_THREE && perm == 3)
        m_splitter.SetSplitterPos(0.0);
    else
    {
        // TM in m_subSplitter pane 0: LT_COLS_THREE perm 1, LT_COLS_SUB_ROWS perm 0
        const bool tmInPane0 = (topo == LT_COLS_THREE && perm == 1) ||
                               (topo == LT_COLS_SUB_ROWS && perm == 0);
        m_subSplitter.SetSplitterPos(tmInPane0 ? 0.0 : 1.0);
    }
}

void CMainFrame::RestoreTreeMapView(bool force)
{
    if (force) GetTreeMapView()->ShowTreeMap(true);
    if (!GetTreeMapView()->IsShowTreeMap()) return;

    const int topo = COptions::LayoutTopology;
    const int perm = COptions::LayoutPermutation;

    if (topo == LT_ROWS_SUB_COLS)
        m_splitter.RestoreSplitterPos(0.5);
    else if (topo == LT_COLS_THREE && perm == 3)
        m_splitter.RestoreSplitterPos(0.40);
    else if (topo == LT_COLS_THREE && perm == 2)
        m_subSplitter.RestoreSplitterPos(1.0 / 3.0);
    else if (topo == LT_COLS_THREE)  // perm 0 or 1
        m_subSplitter.RestoreSplitterPos(0.50);
    else  // LT_COLS_SUB_ROWS
        m_subSplitter.RestoreSplitterPos(0.50);

    GetTreeMapView()->DrawEmptyView();
    GetTreeMapView()->RedrawWindow();
}

LRESULT CMainFrame::OnEnterSizeMove(WPARAM, LPARAM)
{
    GetTreeMapView()->SuspendRecalculationDrawing(true);
    return 0;
}

LRESULT CMainFrame::OnExitSizeMove(WPARAM, LPARAM)
{
    GetTreeMapView()->SuspendRecalculationDrawing(false);
    return 0;
}

void CMainFrame::OnTimer(const UINT_PTR nIDEvent)
{
    // Exit early if shutting down
    if (nIDEvent != ID_WDS_CONTROL || m_shuttingDown) return;

    // Calculate UI updates that do not need to processed frequently
    static unsigned int updateCounter = 0;
    const bool doInfrequentUpdate = updateCounter++ % 15 == 0;
    if (doInfrequentUpdate && !IsIconic())
    {
        // Update memory usage
        UpdatePaneText();
    }

    // UI updates that do need to processed frequently
    if (!CDirStatDoc::Get()->IsRootDone() && !IsScanSuspended())
    {
        // Update the visual progress at the bottom of the screen
        UpdateProgress();

        // By sorting items, items will be redrawn which will
        // also force pacman to update with recent position
        CFileTreeControl::Get()->SortItems();

        // Conditionally sort duplicates
        if (COptions::ScanForDuplicates && doInfrequentUpdate && GetFileTabbedView()->IsFileDupeViewTabActive())
        {
            CFileDupeControl::Get()->SortItems();
        }

        // Conditionally sort largest files
        if (doInfrequentUpdate && GetFileTabbedView()->IsFileTopViewTabActive())
        {
            CFileTopControl::Get()->SortItems();
        }
    }

    CFrameWndEx::OnTimer(nIDEvent);
}

LRESULT CMainFrame::OnCallbackRequest(WPARAM, const LPARAM lParam)
{
    const auto & callback = *static_cast<std::function<void()>*>(std::bit_cast<LPVOID>(lParam));
    callback();
    return 0;
}

void CMainFrame::CopyToClipboard(const std::wstring & psz)
{
    const SIZE_T cchBufLen = psz.size() + 1;
    SmartPointer h(GlobalFree, GlobalAlloc(GMEM_MOVEABLE, cchBufLen * sizeof(WCHAR)));
    if (!h.IsValid())
    {
        DisplayError(TranslateError());
        return;
    }

    // Allocate and copy into global memory
    const HGLOBAL hRaw = h;
    if (SmartPointer lp([hRaw](LPVOID) noexcept { GlobalUnlock(hRaw); }, GlobalLock(hRaw)); lp.IsValid())
    {
        wcscpy_s(static_cast<LPWSTR>(*lp), cchBufLen, psz.c_str());
    }
    else
    {
        DisplayError(TranslateError());
        return;
    }

    // Store text to clipboard
    if (const COpenClipboard clipboard(this);
        !clipboard.IsReady() || SetClipboardData(CF_UNICODETEXT, h) == nullptr)
    {
        DisplayError(TranslateError());
        return;
    }

    // System now owns pointer so do not allow cleanup
    h.Detach();
}

void CMainFrame::OnInitMenuPopup(CMenu* pPopupMenu, const UINT nIndex, const BOOL bSysMenu)
{
    CFrameWndEx::OnInitMenuPopup(pPopupMenu, nIndex, bSysMenu);

    if (const auto [explorerMenu, explorerMenuPos] = LocateNamedMenu(pPopupMenu,
        Localization::Lookup(IDS_MENU_EXPLORER_MENU), false); explorerMenu != nullptr)
    {
        // Add placeholder only
        if (explorerMenu->GetMenuItemCount() == 0)
        {
            explorerMenu->AppendMenu(MF_STRING | MF_DISABLED | MF_GRAYED, 0,
                Localization::Lookup(IDS_PROGRESS).c_str());
        }

        SetMenuItem(pPopupMenu, explorerMenuPos, true);
    }

    // If the menu being opened is populate it
    if (pPopupMenu->GetMenuItemCount() == 1 && pPopupMenu->GetMenuItemID(0) == 0)
    {
        while (pPopupMenu->GetMenuItemCount() > 0)
        {
            pPopupMenu->DeleteMenu(0, MF_BYPOSITION);
        }

        std::vector<std::wstring> paths;
        for (const auto& item : CDirStatDoc::Get()->GetAllSelected())
        {
            paths.push_back(item->GetPath());
        }

        if (const CComPtr contextMenu = GetContextMenu(GetSafeHwnd(), paths);
            contextMenu != nullptr)
        {
            (void) contextMenu->QueryContextMenu(pPopupMenu->GetSafeHmenu(), 0,
                CONTENT_MENU_MINCMD, CONTENT_MENU_MAXCMD, CMF_NORMAL);
        }
    }

    // update cleanup menu if this is the cleanup submenu
    if (pPopupMenu->GetMenuState(ID_CLEANUP_EMPTY_BIN, MF_BYCOMMAND) != static_cast<UINT>(-1))
    {
        UpdateCleanupMenu(pPopupMenu);
    }

    // update tools menu - check if pPopupMenu is the Tools menu by looking for our operation submenus
    const auto [shadowCopyMenu, shadowCopyPos] = LocateNamedMenu(pPopupMenu, Localization::Lookup(IDS_MENU_SHADOW_COPY), false);
    if (shadowCopyMenu != nullptr)
    {
        UpdateToolsMenu(pPopupMenu);
    }
}

void CMainFrame::UpdateCleanupMenu(CMenu* menu, const bool triggerAsync)
{
    // Define menu items structure with cached values
    struct { ULONGLONG* count; ULONGLONG* bytes; UINT menuId; LPCWSTR prefix; } menuItems[] = {
        { &m_recycleBinItems, &m_recycleBinBytes, ID_CLEANUP_EMPTY_BIN, IDS_EMPTY_RECYCLEBIN.data() },
        { &m_shadowCopyCount, &m_shadowCopyBytes, ID_CLEANUP_REMOVE_SHADOW, IDS_MENU_REMOVE_SHADOW.data() }
    };

    // Update menu items using cached values (initially shows zeros or last cached values)
    for (const auto& [count, bytes, menuId, prefix] : menuItems)
    {
        const std::wstring label = Localization::Lookup(prefix) + ((*count == 1) ?
            Localization::Format(IDS_ONEITEMs, FormatBytes(*bytes)) :
            Localization::Format(IDS_sITEMSs, FormatCount(*count), FormatBytes(*bytes)));

        const UINT state = menu->GetMenuState(menuId, MF_BYCOMMAND);
        menu->ModifyMenu(menuId, MF_BYCOMMAND | MF_STRING, menuId, label.c_str());
        menu->EnableMenuItem(menuId, state);
    }

    UpdateDynamicMenuItems(menu);

    // Launch a detached thread to perform the queries
    if (triggerAsync) std::thread([this]()
    {
        // Query recycle bin and shadow copies
        QueryRecycleBin(m_recycleBinItems, m_recycleBinBytes);
        QueryShadowCopies(m_shadowCopyCount, m_shadowCopyBytes);

        // Use InvokeInMessageThread to update the menu on the UI thread
        InvokeInMessageThread([this]()
        {
            // Check if the menu is still valid and visible
            const auto [menuObj, menuPos] = LocateNamedMenu(GetMenu(), Localization::Lookup(IDS_MENU_CLEANUP), false);
            if (menuObj == nullptr || menuObj->GetMenuItemCount() <= 0) return;

            // Update menu items with the newly retrieved values
            UpdateCleanupMenu(menuObj, false);
        });
    }).detach();
}

void CMainFrame::QueryRecycleBin(ULONGLONG& items, ULONGLONG& bytes)
{
    items = 0;
    bytes = 0;

    for (const std::wstring & drive : GetDriveList({DRIVE_FIXED, DRIVE_REMOVABLE, DRIVE_RAMDISK}))
    {
        SHQUERYRBINFO qbi{ .cbSize = sizeof(qbi) };
        if (FAILED(::SHQueryRecycleBin((drive + L"\\").c_str(), &qbi)))
        {
            continue;
        }

        items += qbi.i64NumItems;
        bytes += qbi.i64Size;
    }
}

std::pair<CMenu*,int> CMainFrame::LocateNamedMenu(const CMenu* menu, const std::wstring & subMenuText, const bool removeItems) const
{
    // locate submenu
    CMenu* subMenu = nullptr;
    int subMenuPos = -1;
    for (const int i : std::views::iota(0, menu->GetMenuItemCount()))
    {
        CStringW menuString;
        if (menu->GetMenuString(i, menuString, MF_BYPOSITION) > 0 &&
            _wcsicmp(menuString, subMenuText.c_str()) == 0)
        {
            subMenu = menu->GetSubMenu(i);
            subMenuPos = i;
            break;
        }
    }

    // cleanup old items
    if (removeItems && subMenu != nullptr) while (subMenu->GetMenuItemCount() > 0)
        subMenu->DeleteMenu(0, MF_BYPOSITION);
    return { subMenu, subMenuPos };
}

void CMainFrame::UpdateDynamicMenuItems(CMenu* menu) const
{
    const auto& items = CDirStatDoc::Get()->GetAllSelected();

    // get list of paths from items
    std::vector<std::wstring> paths;
    for (auto& item : items) paths.push_back(item->GetPath());

    // locate compress menu
    auto [compressMenu, compressMenuPos] = CMainFrame::LocateNamedMenu(menu, Localization::Lookup(IDS_MENU_COMPRESS_MENU), false);
    if (compressMenu && compressMenuPos >= 0)
    {
        // Check if any submenu items are enabled
        const int menuItemCount = compressMenu->GetMenuItemCount();
        const bool anyEnabled = std::ranges::any_of(std::views::iota(0, menuItemCount), [&](const int i)
        {
            CCmdUI state;
            state.m_nIndex = i;
            state.m_nIndexMax = menuItemCount;
            state.m_nID = compressMenu->GetMenuItemID(i);
            state.m_pMenu = compressMenu;
            state.DoUpdate(const_cast<CMainFrame*>(this), FALSE);
            return IsMenuEnabled(compressMenu, i);
        });

        SetMenuItem(menu, compressMenuPos, anyEnabled);
    }

    auto[customMenu, customMenuPos] = LocateNamedMenu(menu, Localization::Lookup(IDS_USER_DEFINED_CLEANUP));
    for (UINT iCurrent = 0; customMenu != nullptr && iCurrent < COptions::UserDefinedCleanups.size(); iCurrent++)
    {
        auto& udc = COptions::UserDefinedCleanups[iCurrent];
        if (!udc.Enabled) continue;

        std::wstring string = std::vformat(Localization::Lookup(IDS_UDCsCTRLd),
            std::make_wformat_args(udc.Title.Obj(), iCurrent));

        bool udcValid = GetLogicalFocus() == LF_FILETREE && !items.empty();
        if (udcValid) for (const auto& item : items)
        {
            udcValid &= CDirStatDoc::Get()->UserDefinedCleanupWorksForItem(&udc, item);
        }

        customMenu->AppendMenu(MF_STRING, ID_USERDEFINEDCLEANUP0 + iCurrent, string.c_str());
        SetMenuItem(customMenu, ID_USERDEFINEDCLEANUP0 + iCurrent, udcValid, true);
    }

    // conditionally disable menu if empty
    if (customMenu) SetMenuItem(menu,
        customMenuPos, customMenu->GetMenuItemCount() > 0);
}

void CMainFrame::OnAdvancedShadowCopy(const UINT nID)
{
    const WCHAR driveLetter = wds::strAlpha[nID - ID_TOOLS_SHADOW_COPY_BASE];
    const std::wstring drive = std::format(L"{:c}:", driveLetter);

    bool success = false;
    CProgressDlg dlg(0, true, this, [&](CProgressDlg*)
    {
        success = CreateShadowCopy(drive);
    });
    dlg.DoModal();

    if (!success)
    {
        const std::wstring msg = Localization::Format(IDS_SHADOW_COPY_FAILED, GetDrive(drive));
        WdsMessageBox(*this, msg, wds::strWinDirStat, MB_ICONERROR | MB_OK);
    }
}

void CMainFrame::OnAdvancedDefrag(const UINT nID)
{
    const WCHAR driveLetter = wds::strAlpha[nID - ID_TOOLS_DEFRAG_BASE];
    ExecuteCommandInConsole(std::format(L"DEFRAG.EXE {:c}: /O", driveLetter), L"DEFRAG");
}

void CMainFrame::OnAdvancedChkdsk(const UINT nID)
{
    const WCHAR driveLetter = wds::strAlpha[nID - ID_TOOLS_CHKDSK_BASE];
    ExecuteCommandInConsole(std::format(L"CHKDSK.EXE {:c}: /F", driveLetter), L"CHKDSK");
}

void CMainFrame::UpdateToolsMenu(CMenu* menu)
{
    // menu is the Tools popup menu itself
    // Find each operation submenu and populate with drives
    auto [shadowCopyMenu, shadowCopyPos] = LocateNamedMenu(menu, Localization::Lookup(IDS_MENU_SHADOW_COPY), true);
    auto [defragMenu, defragPos] = LocateNamedMenu(menu, Localization::Lookup(IDS_MENU_DEFRAGMENT), true);
    auto [chkdskMenu, chkdskPos] = LocateNamedMenu(menu, Localization::Lookup(IDS_MENU_CHKDSK), true);

    // Get available local drives and conditionally enable based on elevation
    const auto drives = GetDriveList({DRIVE_FIXED, DRIVE_REMOVABLE, DRIVE_RAMDISK});
    SetMenuItem(menu, shadowCopyPos, IsElevationActive() && !drives.empty());
    SetMenuItem(menu, defragPos, IsElevationPossible() && !drives.empty());
    SetMenuItem(menu, chkdskPos, IsElevationPossible() && !drives.empty());

    for (const auto& drive : drives)
    {
        // Get volume label for display
        const std::wstring volumeName = GetVolumeName(drive);
        const std::wstring displayName = volumeName.empty()
            ? GetDrive(drive) : std::format(L"{:.2} ({})", drive, volumeName);

        const int driveIndex = std::toupper(drive[0]) - L'A';
        shadowCopyMenu->AppendMenu(MF_STRING, ID_TOOLS_SHADOW_COPY_BASE + driveIndex, displayName.c_str());
        defragMenu->AppendMenu(MF_STRING, ID_TOOLS_DEFRAG_BASE + driveIndex, displayName.c_str());
        chkdskMenu->AppendMenu(MF_STRING, ID_TOOLS_CHKDSK_BASE + driveIndex, displayName.c_str());
    }
}

void CMainFrame::SetLogicalFocus(const LOGICAL_FOCUS lf)
{
    if (lf != m_logicalFocus)
    {
        m_logicalFocus = lf;
        UpdatePaneText();

        CDirStatDoc::Get()->UpdateAllViews(nullptr, HINT_SELECTIONSTYLECHANGED);
    }
}

LOGICAL_FOCUS CMainFrame::GetLogicalFocus() const
{
    return m_logicalFocus;
}

void CMainFrame::MoveFocus(const LOGICAL_FOCUS logicalFocus)
{
    switch (logicalFocus)
    {
        case LF_EXTLIST: GetExtensionView()->SetFocus(); break;
        case LF_DUPELIST: GetFileDupeView()->SetFocus(); break;
        case LF_TOPLIST: GetFileTopView()->SetFocus(); break;
        case LF_SEARCHLIST: GetFileSearchView()->SetFocus(); break;
        case LF_WATCHERLIST: GetFileWatcherView()->SetFocus(); break;
        case LF_PERMSLIST: GetFilePermsView()->SetFocus(); break;
        case LF_FILETREE: GetFileTreeView()->SetFocus(); break;
        case LF_NONE:
        {
            SetLogicalFocus(LF_NONE);
            m_wndDeadFocus.SetFocus();
        }
    }
}

void CMainFrame::UpdatePaneText()
{
    const auto focus = GetLogicalFocus();
    std::wstring fileSelectionText = !CDirStatDoc::Get()->IsScanRunning() ?
        Localization::Lookup(IDS_IDLEMESSAGE) : wds::strEmpty;
    ULONGLONG size = MAXULONGLONG;

    // Allow override on hover
    if (const auto [hoverPath, hoverSize] =
        m_treeMapView->GetTreeMapHoverInfo(); !hoverPath.empty())
    {
        fileSelectionText = hoverPath;
        size = hoverSize;
    }

    // Only get the data the document is not actively updating
    else if (CDirStatDoc::Get()->IsRootDone())
    {
        if (focus != LF_EXTLIST)
        {
            const auto& items = CDirStatDoc::Get()->GetAllSelected();
            if (items.size() == 1)
            {
                // If single item selected, show full path
                const auto path = items.front()->GetPath();
                if (!path.empty()) fileSelectionText = path;
            }
            for (size = 0; const auto& item : items)
            {
                size += item->GetSizePhysical();
            }

        }
        else if (fileSelectionText.empty())
        {
            fileSelectionText = wds::chrStar + CDirStatDoc::Get()->GetHighlightExtension();
        }
    }

    // Update select physical size
    const CClientDC dc(this);
    SetStatusPaneText(dc, ID_STATUSPANE_IDLE_INDEX, fileSelectionText);
    SetStatusPaneText(dc, ID_STATUSPANE_SIZE_INDEX, (size == MAXULONGLONG) ? wds::strEmpty :
        std::format(L"{}: \u2211 {}", Localization::Lookup(IDS_COL_SIZE_PHYSICAL), FormatBytes(size)), 175);
    SetStatusPaneText(dc, ID_STATUSPANE_RAM_INDEX, CDirStatApp::GetCurrentProcessMemoryInfo(), 175);
}

void CMainFrame::OnUpdateEnableControl(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(true);
}

void CMainFrame::OnSize(const UINT nType, const int cx, const int cy)
{
    CFrameWndEx::OnSize(nType, cx, cy);

    if (!IsWindow(m_wndStatusBar.m_hWnd))
    {
        return;
    }

    CRect rc;
    m_wndStatusBar.GetItemRect(ID_STATUSPANE_IDLE_INDEX, rc);

    if (m_progress.m_hWnd != nullptr)
    {
        CRect progRc = rc;
        progRc.DeflateRect(DpiRest(3, &m_wndStatusBar), DpiRest(4, &m_wndStatusBar),
            DpiRest(5, &m_wndStatusBar), DpiRest(4, &m_wndStatusBar));
        m_progress.MoveWindow(progRc);
    }
    else if (m_pacman.m_hWnd != nullptr)
    {
        m_pacman.MoveWindow(rc);
    }
}

/////////////////////////////////////////////////////////////////////////////

void CMainFrame::OnUpdateViewShowTreeMap(CCmdUI* pCmdUI)
{
    pCmdUI->SetCheck(GetTreeMapView()->IsShowTreeMap());
}

void CMainFrame::OnUpdateTreeMapUseLogical(CCmdUI* pCmdUI)
{
    pCmdUI->SetCheck(COptions::TreeMapUseLogical);
}

void CMainFrame::OnUpdateViewShowFileTypes(CCmdUI* pCmdUI)
{
    pCmdUI->SetCheck(GetExtensionView()->IsShowTypes());
}

void CMainFrame::OnUpdateViewGroupUnregisteredTypes(CCmdUI* pCmdUI)
{
    // Only allow regrouping when types are shown and the scan has finished
    const CDirStatDoc* doc = CDirStatDoc::Get();
    pCmdUI->Enable(GetExtensionView()->IsShowTypes() && doc->IsRootDone() && !doc->IsScanRunning());
    pCmdUI->SetCheck(COptions::GroupUnregisteredTypes);
}

void CMainFrame::OnUpdateViewShowWatcher(CCmdUI* pCmdUI)
{
    pCmdUI->SetCheck(GetFileTabbedView()->IsWatcherTabVisible());
}

void CMainFrame::OnViewShowTreeMap()
{
    GetTreeMapView()->ShowTreeMap(!GetTreeMapView()->IsShowTreeMap());
    if (GetTreeMapView()->IsShowTreeMap())
    {
        RestoreTreeMapView();
    }
    else
    {
        MinimizeTreeMapView();
    }
}

void CMainFrame::OnViewTreeMapUseLogical()
{
    COptions::TreeMapUseLogical = !COptions::TreeMapUseLogical;
    if (GetTreeMapView()->IsShowTreeMap())
    {
        CDirStatDoc::Get()->RefreshItem(CDirStatDoc::Get()->GetRootItem());
    }
}

void CMainFrame::OnViewShowFileTypes()
{
    GetExtensionView()->ShowTypes(!GetExtensionView()->IsShowTypes());
    if (GetExtensionView()->IsShowTypes())
    {
        RestoreExtensionView();
    }
    else
    {
        MinimizeExtensionView();
    }
}

void CMainFrame::OnViewGroupUnregisteredTypes()
{
    COptions::GroupUnregisteredTypes = !COptions::GroupUnregisteredTypes;

    // Recolor extensions so the unregistered group shares one color, then refresh the list and treemap
    CDirStatDoc::Get()->RebuildExtensionData();
    GetExtensionView()->OnUpdate(nullptr, HINT_NULL, nullptr);
    CDirStatDoc::Get()->UpdateAllViews(nullptr, HINT_TREEMAPSTYLECHANGED);
}

void CMainFrame::OnViewShowExtensionsOnTreeMap()
{
    COptions::TreeMapShowExtensions = !static_cast<bool>(COptions::TreeMapShowExtensions);
    COptions::TreeMapOptions.showExtensions = COptions::TreeMapShowExtensions;
    CDirStatDoc::Get()->UpdateAllViews(nullptr, HINT_TREEMAPSTYLECHANGED);
}

void CMainFrame::OnUpdateViewShowExtensionsOnTreeMap(CCmdUI* pCmdUI)
{
    pCmdUI->SetCheck(COptions::TreeMapOptions.showExtensions);
}

void CMainFrame::OnViewShowFolderFramesOnTreeMap()
{
    COptions::TreeMapShowFolderFrames = !static_cast<bool>(COptions::TreeMapShowFolderFrames);
    COptions::TreeMapOptions.showFolderFrames = COptions::TreeMapShowFolderFrames;
    CDirStatDoc::Get()->UpdateAllViews(nullptr, HINT_TREEMAPSTYLECHANGED);
}

void CMainFrame::OnUpdateViewShowFolderFramesOnTreeMap(CCmdUI* pCmdUI)
{
    pCmdUI->SetCheck(COptions::TreeMapOptions.showFolderFrames);
}

void CMainFrame::RebuildToolBar()
{
    const auto imageSize = COptions::LargeToolBar ? 32 : 20;
    const auto scale = COptions::LargeToolBar ? (32.0f / 20.0f) : 1.0f;

    // Remove all existing buttons
    if (CDirStatApp::Get()->m_pMainWnd == nullptr) return;
    while (m_wndToolBar.GetCount() > 0)
        m_wndToolBar.RemoveButton(0);

    // Clear the shared image list and resize buttons to match the new icon size
    CMFCToolBar::GetImages()->Clear();
    CMFCToolBar::SetSizes(
        { static_cast<LONG>(m_defaultButtonSize.cx * scale),
          static_cast<LONG>(m_defaultButtonSize.cy * scale)},
        { imageSize, imageSize });

    using Painter = std::function<void(Gdiplus::Graphics&)>;
    static const std::vector<std::tuple<UINT, std::wstring_view, Painter>> toolbarButtons =
    {
        { ID_FILE_SELECT,             IDS_FILE_SELECT,             Icons::PaintFileSelect},
        { ID_SEPARATOR,               {},{}},
        { ID_SCAN_RESUME,             IDS_RESUME,                  Icons::Char(L'▶', RGB( 50, 205,  50))},
        { ID_SCAN_SUSPEND,            IDS_SUSPEND,                 Icons::PaintPause},
        { ID_SCAN_STOP,               IDS_STOP,                    Icons::Char(L'■', RGB(220,  20,  60))},
        { ID_SEPARATOR,               {},{}},
        { ID_REFRESH_ALL,             IDS_REFRESH_ALL,             Icons::Char(L'↻', RGB(  0, 156, 221))},
        { ID_REFRESH_SELECTED,        IDS_REFRESH_SELECTED,        Icons::PaintRefreshSelected},
        { ID_SEPARATOR,               {},{}},
        { ID_SEARCH,                  IDS_SEARCH_TITLE,            Icons::Char(L'⌕', Icons::NeutralRef())},
        { ID_FILTER,                  IDS_PAGE_FILTERING_TITLE,    [](auto& g){ Icons::PaintFilter(g, CFiltering::IsFilterActive()); } },
        { ID_SEPARATOR,               {},{}},
        { ID_CLEANUP_OPEN_SELECTED,   IDS_CLEANUP_OPEN_SELECTED,   Icons::PaintOpenSelected},
        { ID_CLEANUP_EXPLORER_SELECT, IDS_CLEANUP_EXPLORER_SELECT, Icons::PaintExplorerSelect},
        { ID_EDIT_COPY_CLIPBOARD,     IDS_EDIT_COPY_CLIPBOARD,     Icons::PaintEditCopyClipboard},
        { ID_CLEANUP_OPEN_IN_CONSOLE, IDS_CLEANUP_OPEN_IN_CONSOLE, Icons::PaintOpenInConsole},
        { ID_CLEANUP_PROPERTIES,      IDS_CLEANUP_PROPERTIES,      Icons::PaintProperties},
        { ID_SEPARATOR,               {},{}},
        { ID_CLEANUP_DELETE_BIN,      IDS_CLEANUP_DELETE_BIN,      Icons::PaintDeleteBin},
        { ID_CLEANUP_DELETE,          IDS_CLEANUP_DELETE,          Icons::PaintDelete},
        { ID_SEPARATOR,               {},{}},
        { ID_TREEMAP_ZOOMIN,          IDS_TREEMAP_ZOOMIN,          [](auto& g) { Icons::PaintMagnifier(g, true);}},
        { ID_TREEMAP_ZOOMOUT,         IDS_TREEMAP_ZOOMOUT,         [](auto& g){ Icons::PaintMagnifier(g, false);}},
        { ID_SEPARATOR,               {},{}},
        { ID_VIEW_WINDOW_LAYOUT,      IDS_WINDOW_LAYOUT,           Icons::PaintWindowLayout},
        { ID_SEPARATOR,               {},{}},
        { ID_CONFIGURE,               IDS_MENU_SETTINGS,           Icons::PaintGear},
        { ID_HELP_MANUAL,             IDS_HELP_MANUAL,             Icons::PaintHelp},
    };

    for (const auto& [id, text, painter] : toolbarButtons)
    {
        if (id == ID_SEPARATOR)
        {
            m_wndToolBar.InsertSeparator();
            continue;
        }

        int index = 0;
        if (painter)
        {
            CBitmap bitmap;
            bitmap.Attach(Icons::MakeBitmap(imageSize, painter));
            index = CMFCToolBar::GetImages()->AddImage(bitmap, TRUE);
        }

        CMFCToolBarButton button(id, index, nullptr, TRUE, TRUE);
        button.m_bText = FALSE;
        button.m_nStyle = TBBS_DISABLED;
        button.m_strText = Localization::Lookup(text).c_str();
        m_wndToolBar.InsertButton(button);
    }

    m_wndToolBar.AdjustLayout();
}

void CMainFrame::OnViewLargeToolBar()
{
    COptions::LargeToolBar = !COptions::LargeToolBar;
    RebuildToolBar();
}

void CMainFrame::OnUpdateViewLargeToolBar(CCmdUI* pCmdUI)
{
    pCmdUI->SetCheck(COptions::LargeToolBar);
    pCmdUI->Enable((m_wndToolBar.GetStyle() & WS_VISIBLE) != 0);
}

void CMainFrame::OnConfigure()
{
    const bool restart = COptionsPropertySheet::ShowSettings();

    // Rebuild the toolbar so icons (e.g. the filter indicator) reflect the new settings
    RebuildToolBar();

    // Save settings in case the application exits abnormally
    PersistedSetting::WritePersistedProperties();

    if (restart)
    {
        CDirStatApp::Get()->RestartApplication();
    }
}

void CMainFrame::OnSysColorChange()
{
    GetFileTreeView()->SysColorChanged();
    GetExtensionView()->SysColorChanged();
    DrawTextCache::Get().ClearCache();

    // Redraw menus for dark mode
    DarkMode::SetAppDarkMode();
    RedrawWindow();
}

UINT CMainFrame::OnPowerBroadcast(UINT, LPARAM)
{
    OnSysColorChange();
    return TRUE;
}

LRESULT CMainFrame::OnUahDrawMenu(WPARAM wParam, LPARAM lParam)
{
    return DarkMode::HandleMenuMessage(GetCurrentMessage()->message, wParam, lParam, *this);
}

void CMainFrame::OnNcPaint()
{
    // Update the bottom of the menu bar that is not properly painted
    CFrameWndEx::OnNcPaint();
    DarkMode::DrawMenuClientArea(*this);
}

BOOL CMainFrame::OnNcActivate(BOOL bActive)
{
    // Update the bottom of the menu bar that is not properly painted
    const auto ret = CFrameWndEx::OnNcActivate(bActive);
    DarkMode::DrawMenuClientArea(*this);
    return ret;
}

BOOL CMainFrame::LoadFrame(const UINT nIDResource, const DWORD dwDefaultStyle, CWnd* pParentWnd, CCreateContext* pContext)
{
    if (!CFrameWndEx::LoadFrame(nIDResource, dwDefaultStyle, pParentWnd, pContext))
    {
        return FALSE;
    }

    Localization::UpdateMenu(*GetMenu());
    Localization::UpdateDialogs(*this);
    SetTitle(wds::strWinDirStat);

    return TRUE;
}

void CMainFrame::OnToolsWatcher()
{
    const bool visible = !GetFileTabbedView()->IsWatcherTabVisible();
    GetFileTabbedView()->SetWatcherTabVisibility(visible);
    if (visible)
    {
        GetFileTabbedView()->SetActiveWatcherView();
    }
}

void CMainFrame::OnToolsPermissions()
{
    GetFileTabbedView()->SetPermsTabVisibility(!GetFileTabbedView()->IsPermsTabVisible());

    // Re-check visibility: a cancelled scan leaves the tab hidden, so don't activate it
    if (GetFileTabbedView()->IsPermsTabVisible())
    {
        GetFileTabbedView()->SetActivePermsView();
    }
}

void CMainFrame::OnUpdateToolsPermissions(CCmdUI* pCmdUI)
{
    // Only allow launching a scan once the file tree has been fully populated
    const auto* doc = CDirStatDoc::Get();
    pCmdUI->SetCheck(GetFileTabbedView()->IsPermsTabVisible());
    pCmdUI->Enable(GetFileTabbedView()->IsPermsTabVisible() ||
        (doc->HasRootItem() && doc->IsRootDone() && !doc->IsScanRunning()));
}

void CMainFrame::OnToolsStorageAnalytics()
{
    GetFileTabbedView()->SetStorageAnalyticsTabVisibility(!GetFileTabbedView()->IsStorageAnalyticsTabVisible());

    if (GetFileTabbedView()->IsStorageAnalyticsTabVisible())
    {
        GetFileTabbedView()->SetActiveStorageAnalyticsView();
    }
}

void CMainFrame::OnUpdateToolsStorageAnalytics(CCmdUI* pCmdUI)
{
    const auto* doc = CDirStatDoc::Get();
    pCmdUI->SetCheck(GetFileTabbedView()->IsStorageAnalyticsTabVisible());
    pCmdUI->Enable(GetFileTabbedView()->IsStorageAnalyticsTabVisible() ||
        (doc->HasRootItem() && doc->IsRootDone() && !doc->IsScanRunning()));
}

void CMainFrame::OnViewWindowLayout()
{
    const int idx = m_wndToolBar.CommandToIndex(ID_VIEW_WINDOW_LAYOUT);
    CRect btnRect;
    m_wndToolBar.GetItemRect(idx, &btnRect);
    m_wndToolBar.ClientToScreen(&btnRect);
    m_layoutPopup.ShowAtButton(btnRect);
}

void CMainFrame::ConfigureSplitterCallbacks(int topo, int perm)
{
    m_splitter.ClearPaneTracking();
    m_subSplitter.ClearPaneTracking();

    auto showTreeMap = [this](bool visible) { GetTreeMapView()->ShowTreeMap(visible); };
    auto showFileTypes = [this](bool visible) { GetExtensionView()->ShowTypes(visible); };

    switch (topo)
    {
    case LT_ROWS_SUB_COLS:
        m_splitter.TrackPane(perm == 0 ? 1 : 0, showTreeMap,
            [this, perm]() { m_splitter.SetSplitterPos(perm == 0 ? 1.0 : 0.0); });
        m_subSplitter.TrackPane(1, showFileTypes,
            [this]() { m_subSplitter.SetSplitterPos(1.0); });
        break;

    case LT_COLS_THREE:
        switch (perm)
        {
        case 0: // [FTV|TM] | ExtV
            m_splitter.TrackPane(1, showFileTypes,
                [this]() { m_splitter.SetSplitterPos(1.0); });
            m_subSplitter.TrackPane(1, showTreeMap,
                [this]() { m_subSplitter.SetSplitterPos(1.0); });
            break;
        case 1: // [TM|FTV] | ExtV
            m_splitter.TrackPane(1, showFileTypes,
                [this]() { m_splitter.SetSplitterPos(1.0); });
            m_subSplitter.TrackPane(0, showTreeMap,
                [this]() { m_subSplitter.SetSplitterPos(0.0); });
            break;
        case 2: // FTV | [ExtV|TM]
            m_subSplitter.TrackPane(0, showFileTypes,
                [this]() { m_subSplitter.SetSplitterPos(0.0); });
            m_subSplitter.TrackPane(1, showTreeMap,
                [this]() { m_subSplitter.SetSplitterPos(1.0); });
            break;
        case 3: // TM | [ExtV|FTV]
            m_splitter.TrackPane(0, showTreeMap,
                [this]() { m_splitter.SetSplitterPos(0.0); });
            m_subSplitter.TrackPane(0, showFileTypes,
                [this]() { m_subSplitter.SetSplitterPos(0.0); });
            break;
        }
        break;

    case LT_COLS_SUB_ROWS:
        m_splitter.TrackPane(1, showFileTypes,
            [this]() { m_splitter.SetSplitterPos(1.0); });
        m_subSplitter.TrackPane(perm == 0 ? 0 : 1, showTreeMap,
            [this, perm]() { m_subSplitter.SetSplitterPos(perm == 0 ? 0.0 : 1.0); });
        break;
    }
}

void CMainFrame::BuildSplitterLayout(int topo, int perm, HWND hFTV, HWND hExtV, HWND hTMV)
{
    auto AttachView = [](CWdsSplitterWnd& splitter, int row, int col, HWND hView)
    {
        ::SetParent(hView, splitter.GetSafeHwnd());
        ::SetWindowLongPtr(hView, GWLP_ID, splitter.IdFromRowCol(row, col));
    };

    switch (topo)
    {
    case LT_ROWS_SUB_COLS:
        m_splitter.CreateStatic(this, 2, 1);
        if (perm == 0) // top: [FTV|ExtV], bottom: TM
        {
            m_subSplitter.CreateStatic(&m_splitter, 1, 2, WS_CHILD | WS_VISIBLE | WS_BORDER,
                                       m_splitter.IdFromRowCol(0, 0));
            AttachView(m_subSplitter, 0, 0, hFTV);
            AttachView(m_subSplitter, 0, 1, hExtV);
            AttachView(m_splitter, 1, 0, hTMV);
        }
        else // perm == 1: TM top, [FTV|ExtV] bottom
        {
            AttachView(m_splitter, 0, 0, hTMV);
            m_subSplitter.CreateStatic(&m_splitter, 1, 2, WS_CHILD | WS_VISIBLE | WS_BORDER,
                                       m_splitter.IdFromRowCol(1, 0));
            AttachView(m_subSplitter, 0, 0, hFTV);
            AttachView(m_subSplitter, 0, 1, hExtV);
        }
        break;

    case LT_COLS_THREE:
        m_splitter.CreateStatic(this, 1, 2);
        if (perm == 0) // [FTV|TM] in col 0, ExtV in col 1
        {
            m_subSplitter.CreateStatic(&m_splitter, 1, 2, WS_CHILD | WS_VISIBLE | WS_BORDER,
                                       m_splitter.IdFromRowCol(0, 0));
            AttachView(m_subSplitter, 0, 0, hFTV);
            AttachView(m_subSplitter, 0, 1, hTMV);
            AttachView(m_splitter, 0, 1, hExtV);
        }
        else if (perm == 1) // [TM|FTV] in col 0, ExtV in col 1
        {
            m_subSplitter.CreateStatic(&m_splitter, 1, 2, WS_CHILD | WS_VISIBLE | WS_BORDER,
                                       m_splitter.IdFromRowCol(0, 0));
            AttachView(m_subSplitter, 0, 0, hTMV);
            AttachView(m_subSplitter, 0, 1, hFTV);
            AttachView(m_splitter, 0, 1, hExtV);
        }
        else if (perm == 2) // FTV in col 0, [ExtV|TM] in col 1
        {
            AttachView(m_splitter, 0, 0, hFTV);
            m_subSplitter.CreateStatic(&m_splitter, 1, 2, WS_CHILD | WS_VISIBLE | WS_BORDER,
                                       m_splitter.IdFromRowCol(0, 1));
            AttachView(m_subSplitter, 0, 0, hExtV);
            AttachView(m_subSplitter, 0, 1, hTMV);
        }
        else // perm 3: TM in col 0, [ExtV|FTV] in col 1
        {
            AttachView(m_splitter, 0, 0, hTMV);
            m_subSplitter.CreateStatic(&m_splitter, 1, 2, WS_CHILD | WS_VISIBLE | WS_BORDER,
                                       m_splitter.IdFromRowCol(0, 1));
            AttachView(m_subSplitter, 0, 0, hExtV);
            AttachView(m_subSplitter, 0, 1, hFTV);
        }
        break;

    case LT_COLS_SUB_ROWS:
        m_splitter.CreateStatic(this, 1, 2);
        m_subSplitter.CreateStatic(&m_splitter, 2, 1, WS_CHILD | WS_VISIBLE | WS_BORDER,
                                   m_splitter.IdFromRowCol(0, 0));
        if (perm == 0) // left: TM/FTV; right: ExtV
        {
            AttachView(m_subSplitter, 0, 0, hTMV);
            AttachView(m_subSplitter, 1, 0, hFTV);
        }
        else // perm 1: left: FTV/TM; right: ExtV
        {
            AttachView(m_subSplitter, 0, 0, hFTV);
            AttachView(m_subSplitter, 1, 0, hTMV);
        }
        AttachView(m_splitter, 0, 1, hExtV);
        break;
    }
}

void CMainFrame::RebuildLayout(bool resetPositions)
{
    int topo = COptions::LayoutTopology;
    int perm = COptions::LayoutPermutation;
    const bool isDefault = (topo == LT_ROWS_SUB_COLS && perm == 0);
    if (topo == 1 || (!isDefault && CLayoutPopup::LayoutIndex(topo, perm) == 0))
    {
        topo = LT_ROWS_SUB_COLS;
        perm = 0;
        COptions::LayoutTopology = topo;
        COptions::LayoutPermutation = perm;
    }

    // Capture view HWNDs; reparent to frame so DestroyWindow below doesn't kill them.
    const HWND hFTV   = GetFileTabbedView()->GetSafeHwnd();
    const HWND hExtV  = GetExtensionView()->GetSafeHwnd();
    const HWND hTMV   = GetTreeMapView()->GetSafeHwnd();
    const HWND hFrame = GetSafeHwnd();
    ::SetParent(hFTV,  hFrame);
    ::SetParent(hExtV, hFrame);
    ::SetParent(hTMV,  hFrame);

    if (m_splitter.GetSafeHwnd())
        m_splitter.DestroyWindow();

    if (resetPositions)
    {
        COptions::MainSplitterPos = -1.0;
        COptions::SubSplitterPos  = -1.0;
    }

    BuildSplitterLayout(topo, perm, hFTV, hExtV, hTMV);
    ::ShowWindow(hExtV, SW_SHOW);

    ConfigureSplitterCallbacks(topo, perm);
    m_splitter.SetStorage(COptions::MainSplitterPos.Ptr());
    m_subSplitter.SetStorage(COptions::SubSplitterPos.Ptr());
    RecalcLayout();

    switch (topo)
    {
    case LT_ROWS_SUB_COLS:
        m_splitter.RestoreSplitterPos(0.5);
        m_subSplitter.RestoreSplitterPos(0.75);
        break;
    case LT_COLS_THREE:
        if (perm == 0 || perm == 1)
        {
            m_splitter.RestoreSplitterPos(0.80);
            m_subSplitter.RestoreSplitterPos(0.50);
        }
        else // perm 2, 3: first col = 40%, sub in col 1 gets 60%
        {
            m_splitter.RestoreSplitterPos(0.40);
            m_subSplitter.RestoreSplitterPos(1.0 / 3.0);
        }
        break;
    case LT_COLS_SUB_ROWS:
        m_splitter.RestoreSplitterPos(0.75);
        m_subSplitter.RestoreSplitterPos(0.50);
        break;
    }

    if (!GetExtensionView()->IsShowTypes())
        MinimizeExtensionView();
    if (!GetTreeMapView()->IsShowTreeMap())
        MinimizeTreeMapView();

    DarkMode::AdjustControls(GetSafeHwnd());
    GetFileTabbedView()->RedrawWindow();
    GetTreeMapView()->RedrawWindow();
    GetExtensionView()->RedrawWindow();
}
