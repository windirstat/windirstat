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
#include "FlameGraphView.h"
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

bool COptionsPropertySheet::ShowSettings(const int initialPage, const bool refreshOnFilteringChange)
{
    auto sheet = std::make_unique<COptionsPropertySheet>();
    sheet->m_initialPage = initialPage; // -1 means restore last-used tab

    auto general = std::make_unique<CPageGeneral>();
    auto filtering = std::make_unique<CPageFiltering>(refreshOnFilteringChange);
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

BOOL CWdsSplitterWnd::PreCreateWindow(CREATESTRUCT& cs)
{
    cs.style |= WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
    return CSplitterWndEx::PreCreateWindow(cs);
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
    ON_COMMAND(ID_VIEW_SHOWTREEMAP, OnViewTreeMap)
    ON_COMMAND(ID_VIEW_FLAMEGRAPH, OnViewFlameGraph)
    ON_COMMAND(ID_TREEMAP_LOGICAL_SIZE, OnViewTreeMapUseLogical)
    ON_MESSAGE(WM_ENTERSIZEMOVE, OnEnterSizeMove)
    ON_MESSAGE(WM_EXITSIZEMOVE, OnExitSizeMove)
    ON_MESSAGE(WM_CALLBACKUI, OnCallbackRequest)
    ON_MESSAGE(DarkMode::WM_UAHDRAWMENU, OnUahDrawMenu)
    ON_MESSAGE(DarkMode::WM_UAHDRAWMENUITEM, OnUahDrawMenu)
    ON_REGISTERED_MESSAGE(s_TaskBarMessage, OnTaskButtonCreated)
    ON_UPDATE_COMMAND_UI(ID_VIEW_SHOWTREEMAP, OnUpdateViewShowTreeMap)
    ON_UPDATE_COMMAND_UI(ID_VIEW_FLAMEGRAPH, OnUpdateViewFlameGraph)
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
    ON_WM_ERASEBKGND()
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
    ON_COMMAND(ID_WATCHER_START, &CMainFrame::OnWatcherStart)
    ON_UPDATE_COMMAND_UI(ID_WATCHER_START, &CMainFrame::OnUpdateWatcherStart)
    ON_COMMAND(ID_WATCHER_PAUSE, &CMainFrame::OnWatcherPause)
    ON_UPDATE_COMMAND_UI(ID_WATCHER_PAUSE, &CMainFrame::OnUpdateWatcherPause)
    ON_COMMAND(ID_WATCHER_AUTOSCROLL, &CMainFrame::OnWatcherAutoScroll)
    ON_UPDATE_COMMAND_UI(ID_WATCHER_AUTOSCROLL, &CMainFrame::OnUpdateWatcherAutoScroll)
    ON_COMMAND(ID_WATCHER_CLEAR, &CMainFrame::OnWatcherClear)
    ON_UPDATE_COMMAND_UI(ID_WATCHER_CLEAR, &CMainFrame::OnUpdateWatcherClear)
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

BOOL CMainFrame::OnEraseBkgnd(CDC* /*pDC*/)
{
    return TRUE;
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
    CWinDirStatModel::Get()->SetScanTitlePrefix(wds::strEmpty);
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
    const auto currentRoot = CWinDirStatModel::Get()->GetRootItem();
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
    CWinDirStatModel::Get()->SetScanTitlePrefix(titlePrefix);
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

    if (DarkMode::IsDarkModeActive())
    {
        static CBrush s_darkBkgndBrush;
        if (s_darkBkgndBrush.GetSafeHandle() == nullptr)
        {
            s_darkBkgndBrush.CreateSolidBrush(DarkMode::WdsSysColor(COLOR_WINDOW));
        }
        SetClassLongPtr(GetSafeHwnd(), GCLP_HBRBACKGROUND, reinterpret_cast<LONG_PTR>(s_darkBkgndBrush.GetSafeHandle()));
    }

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
    CWinDirStatModel::Get()->StopScanningEngine(CWinDirStatModel::Abort);

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
    COptions::ShowTreeMap = IsActiveGraphPaneShown();

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

    m_flameGraphView = new CFlameGraphView();
    if (!m_flameGraphView->Create(nullptr, nullptr, WS_CHILD | WS_VSCROLL, CRect{}, this, 0))
    {
        // Create invokes PostNcDestroy on failure, which deletes the view.
        m_flameGraphView = nullptr;
        return FALSE;
    }

    GetExtensionView()->ShowTypes(COptions::ShowFileTypes);
    GetTreeMapView()->ShowTreeMap(COptions::ShowTreeMap);
    m_flameGraphView->ShowTreeMap(COptions::ShowTreeMap);

    m_layoutPopup.Create(this);
    RebuildLayout();
    return TRUE;
}

void CMainFrame::UpdateAllPanes(CWnd* sender, MODEL_CHANGE change, CItem* item)
{
    const std::array<CWinDirStatPane*, 4> panes{
        m_fileTabbedView, m_extensionView, m_treeMapView, m_flameGraphView
    };
    for (CWinDirStatPane* pane : panes)
    {
        if (pane != nullptr && pane != sender)
        {
            pane->OnUpdate(sender, change, item);
        }
    }
}

void CMainFrame::UpdateFrameTitleForScan(LPCWSTR scanName)
{
    UpdateFrameTitleForDocument(scanName);
}

BOOL CMainFrame::OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo)
{
    if (CWnd* focus = GetFocus(); focus != nullptr && focus != this && this->IsChild(focus))
    {
        for (CWnd* target = focus; target != nullptr && target != this; target = target->GetParent())
        {
            if (target->OnCmdMsg(nID, nCode, pExtra, pHandlerInfo))
            {
                return TRUE;
            }
        }
    }

    if (CFrameWndEx::OnCmdMsg(nID, nCode, pExtra, pHandlerInfo))
    {
        return TRUE;
    }

    if (CWinDirStatModel::Get()->OnCmdMsg(nID, nCode, pExtra, pHandlerInfo))
    {
        return TRUE;
    }

    if (CWinApp* app = AfxGetApp();
        app != nullptr && app->OnCmdMsg(nID, nCode, pExtra, pHandlerInfo))
    {
        return TRUE;
    }

    return FALSE;
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
    // LT_COLS_TM_FULL: ExtV is in m_subSplitter row 0 (perm 0/2) or row 1 (perm 1/3)
    else if (topo == LT_COLS_TM_FULL)
        m_subSplitter.SetSplitterPos(perm == 0 || perm == 2 ? 0.0 : 1.0);
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
    else if (topo == LT_COLS_TM_FULL)
        m_subSplitter.RestoreSplitterPos(0.50);
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
                             (topo == LT_COLS_THREE   && perm == 3) ||
                             (topo == LT_COLS_TM_FULL && (perm == 0 || perm == 1));
    m_splitter.SetSplitterPos(ftvInPane1 ? 0.0 : 1.0);

    // LT_COLS_THREE perm 2: FTV is directly in main splitter, no sub-splitter needed.
    if (topo == LT_COLS_THREE && perm == 2)
        return;

    // Collapse whichever sub-splitter pane doesn't contain FTV.
    const bool ftvInSubPane1 = (topo == LT_COLS_THREE    && (perm == 1 || perm == 3)) ||
                                (topo == LT_COLS_SUB_ROWS && perm == 0) ||
                                (topo == LT_COLS_TM_FULL  && (perm == 0 || perm == 2));
    m_subSplitter.SetSplitterPos(ftvInSubPane1 ? 0.0 : 1.0);
}

void CMainFrame::MinimizeGraphPane()
{
    const int topo = COptions::LayoutTopology;
    const int perm = COptions::LayoutPermutation;

    if (topo == LT_ROWS_SUB_COLS)
        m_splitter.SetSplitterPos(perm == 0 ? 1.0 : 0.0);
    else if (topo == LT_COLS_THREE && perm == 3)
        m_splitter.SetSplitterPos(0.0);
    else if (topo == LT_COLS_TM_FULL)
        m_splitter.SetSplitterPos(perm == 0 || perm == 1 ? 0.0 : 1.0);
    else
    {
        // Graph in m_subSplitter pane 0: LT_COLS_THREE perm 1, LT_COLS_SUB_ROWS perm 0
        const bool graphInPane0 = (topo == LT_COLS_THREE && perm == 1) ||
                                  (topo == LT_COLS_SUB_ROWS && perm == 0);
        m_subSplitter.SetSplitterPos(graphInPane0 ? 0.0 : 1.0);
    }
}

void CMainFrame::RestoreGraphPane(bool force)
{
    if (force) ShowActiveGraphPane(true);
    if (!IsActiveGraphPaneShown()) return;

    const int topo = COptions::LayoutTopology;
    const int perm = COptions::LayoutPermutation;

    if (topo == LT_ROWS_SUB_COLS)
        m_splitter.RestoreSplitterPos(0.5);
    else if (topo == LT_COLS_TM_FULL)
        m_splitter.RestoreSplitterPos(0.50);
    else if (topo == LT_COLS_THREE && perm == 3)
        m_splitter.RestoreSplitterPos(0.40);
    else if (topo == LT_COLS_THREE && perm == 2)
        m_subSplitter.RestoreSplitterPos(1.0 / 3.0);
    else if (topo == LT_COLS_THREE)  // perm 0 or 1
        m_subSplitter.RestoreSplitterPos(0.50);
    else  // LT_COLS_SUB_ROWS
        m_subSplitter.RestoreSplitterPos(0.50);

    if (COptions::UseFlameGraph)
    {
        m_flameGraphView->DrawEmptyView();
        m_flameGraphView->RedrawWindow();
    }
    else
    {
        m_treeMapView->DrawEmptyView();
        m_treeMapView->RedrawWindow();
    }
}

void CMainFrame::ShowActiveGraphPane(const bool show)
{
    if (COptions::UseFlameGraph)
        m_flameGraphView->ShowTreeMap(show);
    else
        m_treeMapView->ShowTreeMap(show);
}

bool CMainFrame::IsActiveGraphPaneShown() const
{
    return COptions::UseFlameGraph
        ? m_flameGraphView->IsShowTreeMap()
        : m_treeMapView->IsShowTreeMap();
}

CWinDirStatPane* CMainFrame::GetActiveGraphPane() const
{
    return COptions::UseFlameGraph
        ? static_cast<CWinDirStatPane*>(m_flameGraphView)
        : static_cast<CWinDirStatPane*>(m_treeMapView);
}

LRESULT CMainFrame::OnEnterSizeMove(WPARAM, LPARAM)
{
    GetActiveGraphPane()->SuspendRecalculationDrawing(true);
    return 0;
}

LRESULT CMainFrame::OnExitSizeMove(WPARAM, LPARAM)
{
    GetActiveGraphPane()->SuspendRecalculationDrawing(false);
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
    if (!CWinDirStatModel::Get()->IsRootDone() && !IsScanSuspended())
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
