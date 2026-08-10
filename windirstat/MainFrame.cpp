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
#include "VisualizationPane.h"
#include "FileTabbedView.h"
#include "FileTreeView.h"
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

/////////////////////////////////////////////////////////////////////////////

CSettingsSheet::CSettingsSheet()
    : MessageTarget(Localization::Lookup(IDS_WINDIRSTAT_SETTINGS).c_str())
{
}

void CSettingsSheet::SetRestartRequired(const bool changed)
{
    m_restartRequest = changed;
}

bool CSettingsSheet::OnEraseBkgnd(CDC* pDC) const
{
    if (!DarkMode::IsDarkModeActive())
    {
        return CPropertySheet::OnEraseBkgnd(pDC);
    }

    // Paint the background with dark mode color
    const CRect rect = ClientRect();
    pDC->FillSolidRect(&rect, DarkMode::SystemColor(COLOR_WINDOW));

    return true;
}

HBRUSH CSettingsSheet::OnCtlColor(CDC* pDC, CWnd* pWnd, const UINT nCtlColor)
{
    const HBRUSH brush = DarkMode::OnCtlColor(pDC, nCtlColor);
    return brush ? brush : CPropertySheet::OnCtlColor(pDC, pWnd, nCtlColor);
}

bool CSettingsSheet::OnInitDialog()
{
    if (!CPropertySheet::OnInitDialog()) return false;
    CTabCtrlHelper::SetupTabControl(TabControl());

    Localization::UpdateDialogs(*this);
    Localization::UpdateTabControl(TabControl());
    DarkMode::AdjustControls(Handle());

    const int page = (m_initialPage >= 0) ? m_initialPage : static_cast<int>(COptions::ConfigPage);
    SelectPage(std::min(static_cast<int>(page), PageCount() - 1));
    return true;
}

bool CSettingsSheet::ShowSettings(const int initialPage, const bool refreshOnFilteringChange)
{
    CSettingsSheet sheet;
    sheet.m_initialPage = initialPage; // -1 means restore last-used tab
    sheet.AddPage<CPageGeneral>();
    sheet.AddPage<CPageFiltering>(refreshOnFilteringChange); // index 1
    sheet.AddPage<CPageFileTree>();
    sheet.AddPage<CPageTreeMap>();
    sheet.AddPage<CPagePermissions>();
    sheet.AddPage<CPageCleanups>();
    sheet.AddPage<CPagePrompts>();
    sheet.AddPage<CPageAdvanced>();

    sheet.ShowModal();
    return sheet.m_restartApplication;
}

bool CSettingsSheet::OnCommand(const WPARAM wParam, const LPARAM lParam)
{
    COptions::ConfigPage = ActivePageIndex();

    if (const UINT cmd = LOWORD(wParam); IDOK == cmd || ID_APPLY_NOW == cmd)
    {
        if (m_restartRequest && (IDOK == cmd || !m_alreadyAsked))
        {
            const int r = ShowMessageBox(*this, Localization::Lookup(IDS_RESTART_REQUEST),
                wds::strWinDirStat, MB_YESNOCANCEL);
            if (IDCANCEL == r)
            {
                return true; // "Message handled". Don't proceed.
            }
            if (IDNO == r)
            {
                m_alreadyAsked = true; // Don't ask twice.
            }
            else
            {
                assert(IDYES == r);
                m_restartApplication = true;

                if (ID_APPLY_NOW == cmd)
                {
                    // Exit after the base handler applies the modified pages
                    RequestModalExit(IDOK);
                }
            }
        }
    }

    return CPropertySheet::OnCommand(wParam, lParam);
}

/////////////////////////////////////////////////////////////////////////////

CWdsSplitterWnd::CWdsSplitterWnd(double* splitterPos) :
    m_userSplitterPos(splitterPos)
{
    m_wasTrackedByUser = (*splitterPos > 0 && *splitterPos < 1);
}

bool CWdsSplitterWnd::PreCreateWindow(CREATESTRUCT& cs)
{
    cs.style |= WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
    return CSplitterWnd::PreCreateWindow(cs);
}

void CWdsSplitterWnd::PostNcDestroy()
{
    // Reset row/column state before the embedded splitter is created again.
    ResetPanes();
    CSplitterWnd::PostNcDestroy();
}

void CWdsSplitterWnd::StopTracking(const bool bAccept)
{
    CSplitterWnd::StopTracking(bAccept);
    if (!bAccept) return;

    const bool isVertical = ColumnCount() > 1;
    const int currentPos = isVertical ? ColumnSize(0) : RowSize(0);

    const CRect rcClient  = ClientRect();
    const int   totalSize = isVertical ? rcClient.Width() : rcClient.Height();
    if (totalSize <= 0) return;

    const int paneSize[2] = { currentPos, totalSize - currentPos };
    for (int pane = 0; pane < 2; ++pane)
    {
        const auto& [onToggle, onMinimize] = m_paneTracking[pane];
        if (!onToggle) continue;

        const bool isVisible = paneSize[pane] > ScaleForDpi(COptions::MinimizeViewThreshold);
        onToggle(isVisible);

        if (!isVisible)
        {
            if (onMinimize) onMinimize();
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
    assert(pane == 0 || pane == 1);
    if (pane == 0 || pane == 1)
        m_paneTracking[pane] = { std::move(onToggle), std::move(onMinimize) };
}

void CWdsSplitterWnd::SetSplitterPos(const double pos)
{
    m_splitterPos = pos;
    const CRect rc = ClientRect();
    if (ColumnCount() > 1)
    {
        if (const int cx = static_cast<int>(pos * rc.Width()); cx >= 0) { SetColumnSize(0, cx); UpdateLayout(); }
    }
    else
    {
        if (const int cy = static_cast<int>(pos * rc.Height()); cy >= 0) { SetRowSize(0, cy); UpdateLayout(); }
    }
}

void CWdsSplitterWnd::RestoreSplitterPos(const double posIfVirgin)
{
    SetSplitterPos(m_wasTrackedByUser ? *m_userSplitterPos : posIfVirgin);
}

void CWdsSplitterWnd::OnSize(const UINT nType, const int cx, const int cy)
{
    if (ColumnCount() > 1)
    {
        if (const int v = static_cast<int>(cx * m_splitterPos); v > 0) SetColumnSize(0, v);
    }
    else
    {
        if (const int v = static_cast<int>(cy * m_splitterPos); v > 0) SetRowSize(0, v);
    }
    CSplitterWnd::OnSize(nType, cx, cy);
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

bool CPacmanControl::OnEraseBkgnd(CDC* pDC)
{
    UNREFERENCED_PARAMETER(pDC);
    return true;
}

void CPacmanControl::OnPaint()
{
    CPaintDC paintDC(this);
    CBufferedDC dc(paintDC, this);

    // Draw the animation
    const CRect rc = ClientRect();
    m_pacman.Draw(&dc, rc, DarkMode::SystemColor(
        DarkMode::IsDarkModeActive() ? COLOR_WINDOW : COLOR_BTNFACE));

    // Draw the borders

    CStatusBar::DrawPaneBorder(dc, rc);

}

UINT CMainFrame::s_TaskBarMessage = ::RegisterWindowMessage(L"TaskbarButtonCreated");

CMainFrame::CMainFrame()
{
    s_Singleton = this;
}

CMainFrame::~CMainFrame()
{
    s_Singleton = nullptr;
}

bool CMainFrame::OnEraseBkgnd(CDC* /*pDC*/)
{
    return true;
}

void CMainFrame::OnSetFocus(CWnd* pOldWnd)
{
    CFrameWnd::OnSetFocus(pOldWnd);
    if (::GetFocus() == m_hWnd && GetLogicalFocus() != LF_NONE)
    {
        MoveFocus(GetLogicalFocus());
    }
}

void CMainFrame::OnKeyDown(const UINT nChar, const UINT nRepCnt, const UINT nFlags)
{
    if (nChar == VK_TAB)
    {
        MoveFocus(LF_FILETREE);
        return;
    }
    CFrameWnd::OnKeyDown(nChar, nRepCnt, nFlags);
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
        else if (!suspend && (m_taskbarButtonState & TBPF_PAUSED) != 0)
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
        CRect rc = m_wndStatusBar.PaneRect(CStatusBar::PaneId::Idle);
        rc.Deflate(m_wndStatusBar.ScaleForDpi(3), m_wndStatusBar.ScaleForDpi(4),
            m_wndStatusBar.ScaleForDpi(5), m_wndStatusBar.ScaleForDpi(4));
        rc.right = std::max(rc.left, rc.right);
        rc.bottom = std::max(rc.top, rc.bottom);
        m_progress.Create(WS_CHILD | WS_VISIBLE, rc, &m_wndStatusBar, ID_WDS_CONTROL);
        m_progress.ModifyStyle(WS_BORDER, 0);

        if (DarkMode::IsDarkModeActive())
        {
            // Disable theming for progress bar to avoid light background in dark mode
            SetWindowTheme(m_progress.Handle(), L"", L"");
            m_progress.SetBkColor(DarkMode::SystemColor(COLOR_WINDOWFRAME));
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
        const CRect rc = m_wndStatusBar.PaneRect(CStatusBar::PaneId::Idle);
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

void CMainFrame::SetStatusPaneText(const CDC& cdc, const CStatusBar::PaneId pane,
    const std::wstring & text, const int minWidth)
{
    // set status path width and then set text
    const auto cx = cdc.GetTextExtent(text.c_str(), static_cast<int>(text.size())).cx;
    m_wndStatusBar.SetPaneContent(pane, text, std::max(static_cast<int>(cx), ScaleForDpi(minWidth)));
}

int CMainFrame::OnCreate(const LPCREATESTRUCT lpCreateStruct)
{
    if (CFrameWnd::OnCreate(lpCreateStruct) == -1)
    {
        return -1;
    }

    // Setup status pane and force initial field population
    m_wndStatusBar.Create(this);

    UpdatePaneText();

    // Setup status pane for dark mode
    if (DarkMode::IsDarkModeActive())
    {
        m_wndStatusBar.SetBackgroundColor(DarkMode::SystemColor(COLOR_WINDOW));

    }

    m_wndToolBar.Create(this);

    // Save the unscaled default button size before any SetMetrics call
    m_defaultButtonSize = m_wndToolBar.ButtonSize();
    RebuildToolBar();

    // Show or hide status bar if requested
    if (!COptions::ShowStatusBar) m_wndStatusBar.ShowWindow(SW_HIDE);
    if (!COptions::ShowToolBar) m_wndToolBar.ShowWindow(SW_HIDE);

    // apply dark mode to main frame window
    DarkMode::AdjustControls(Handle());

    if (DarkMode::IsDarkModeActive())
    {
        static CBrush s_darkBkgndBrush(DarkMode::SystemColor(COLOR_WINDOW));
        SetClassLongPtr(Handle(), GCLP_HBRBACKGROUND, reinterpret_cast<LONG_PTR>(s_darkBkgndBrush.Handle()));
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

    SetTimer(ID_WDS_CONTROL, 25);
}

void CMainFrame::InvokeInMessageThread(std::function<void()> callback) const
{
    if (m_ownerThreadId == GetCurrentThreadId()) callback();
    else Get()->SendMessage(WM_CALLBACKUI, 0, &callback);
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

    CFrameWnd::OnClose();
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
    COptions::ShowVisualization = IsVisualizationShown();

    // Close all artifacts and our child windows
    CFrameWnd::OnDestroy();

    // Persist values at very end after all children have closed
    PersistedSetting::WritePersistedProperties();
}

bool CMainFrame::OnCreateClient()
{
    if (!m_splitter.CreateStatic(this, 2, 1)
        || !m_splitter.CreateView<CVisualizationPane>(1, 0, CSize(100, 100))
        || !m_subSplitter.CreateStatic(&m_splitter, 1, 2, WS_CHILD | WS_VISIBLE | WS_BORDER,
            m_splitter.PaneId(0, 0))
        || !m_subSplitter.CreateView<CFileTabbedView>(0, 0, CSize(700, 500))
        || !m_subSplitter.CreateView<CExtensionView>(0, 1, CSize(100, 500)))
    {
        return false;
    }

    m_visualizationPane = static_cast<CVisualizationPane*>(m_splitter.PaneAt(1, 0));
    m_fileTabbedView = static_cast<CFileTabbedView*>(m_subSplitter.PaneAt(0, 0));
    m_extensionView = static_cast<CExtensionView*>(m_subSplitter.PaneAt(0, 1));
    if (m_visualizationPane == nullptr || m_fileTabbedView == nullptr || m_extensionView == nullptr)
        return false;

    GetExtensionView()->ShowTypes(COptions::ShowFileTypes);

    m_layoutPopup.Create(this);
    RebuildLayout();
    return true;
}

void CMainFrame::UpdateAllPanes(CWnd* sender, const MODEL_CHANGE change, CItem* item) const
{
    const std::array<CWinDirStatPane*, 3> panes{
        m_fileTabbedView, m_extensionView, m_visualizationPane
    };
    for (CWinDirStatPane* pane : panes)
    {
        if (pane != nullptr && pane != sender)
        {
            pane->OnUpdate(sender, change, item);
        }
    }
}

void CMainFrame::UpdateFrameTitleForScan(const LPCWSTR scanName)
{
    SetDocumentTitle(scanName);
}

bool CMainFrame::PreCreateWindow(CREATESTRUCT& cs)
{
    // seed initial Title bar text
    static std::wstring title = std::format(L"{}{}", GetAppTitle(),
        IsElevationActive() ? std::format(L" ({})", Localization::Lookup(IDS_ADMIN)) : wds::strEmpty);
    cs.lpszName = title.c_str();

    if (!CFrameWnd::PreCreateWindow(cs))
    {
        return false;
    }

    // Prevent flashing of the main window when launching in non-interactive mode
    if (!CDirStatApp::Get()->GetSaveToPath().empty() ||
        !CDirStatApp::Get()->GetSaveDupesToPath().empty() ||
        !CDirStatApp::Get()->GetSavePermsToPath().empty())
    {
        CDirStatApp::Get()->m_nCmdShow = SW_HIDE;
        cs.style &= ~WS_VISIBLE;
        cs.dwExStyle |= WS_EX_NOACTIVATE;
    }

    return true;
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
    // LT_COLS_VISUALIZATION_FULL: ExtV is in sub-splitter row 0 (perm 0/2) or row 1 (perm 1/3)
    else if (topo == LT_COLS_VISUALIZATION_FULL)
        m_subSplitter.SetSplitterPos(perm == 0 || perm == 2 ? 0.0 : 1.0);
    // LT_ROWS_SUB_COLS: ExtV is in m_subSplitter col 1
    else
        m_subSplitter.SetSplitterPos(1.0);
}

void CMainFrame::ExpandFileTabbedView()
{
    const int topo = COptions::LayoutTopology;
    const int perm = COptions::LayoutPermutation;

    // Collapse whichever main-splitter pane doesn't contain FTV.
    const bool ftvInPane1 = (topo == LT_ROWS_SUB_COLS && perm == 1) ||
                             (topo == LT_COLS_THREE   && perm == 3) ||
                             (topo == LT_COLS_VISUALIZATION_FULL && (perm == 0 || perm == 1));
    m_splitter.SetSplitterPos(ftvInPane1 ? 0.0 : 1.0);

    // LT_COLS_THREE perm 2: FTV is directly in main splitter, no sub-splitter needed.
    if (topo == LT_COLS_THREE && perm == 2)
        return;

    // Collapse whichever sub-splitter pane doesn't contain FTV.
    const bool ftvInSubPane1 = (topo == LT_COLS_THREE    && (perm == 1 || perm == 3)) ||
                                (topo == LT_COLS_SUB_ROWS && perm == 0) ||
                                (topo == LT_COLS_VISUALIZATION_FULL && (perm == 0 || perm == 2));
    m_subSplitter.SetSplitterPos(ftvInSubPane1 ? 0.0 : 1.0);
}

void CMainFrame::MinimizeVisualizationPane()
{
    const int topo = COptions::LayoutTopology;
    const int perm = COptions::LayoutPermutation;

    if (topo == LT_ROWS_SUB_COLS)
        m_splitter.SetSplitterPos(perm == 0 ? 1.0 : 0.0);
    else if (topo == LT_COLS_THREE && perm == 3)
        m_splitter.SetSplitterPos(0.0);
    else if (topo == LT_COLS_VISUALIZATION_FULL)
        m_splitter.SetSplitterPos(perm == 0 || perm == 1 ? 0.0 : 1.0);
    else
    {
        // Visualization in sub-splitter pane 0: LT_COLS_THREE perm 1, LT_COLS_SUB_ROWS perm 0
        const bool visualizationInPane0 = (topo == LT_COLS_THREE && perm == 1)
            || (topo == LT_COLS_SUB_ROWS && perm == 0);
        m_subSplitter.SetSplitterPos(visualizationInPane0 ? 0.0 : 1.0);
    }
}

void CMainFrame::RestoreSplitterPositions()
{
    const int topo = COptions::LayoutTopology;
    const int perm = COptions::LayoutPermutation;

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
        else
        {
            m_splitter.RestoreSplitterPos(0.40);
            m_subSplitter.RestoreSplitterPos(1.0 / 3.0);
        }
        break;

    case LT_COLS_SUB_ROWS:
        m_splitter.RestoreSplitterPos(0.75);
        m_subSplitter.RestoreSplitterPos(0.50);
        break;

    case LT_COLS_VISUALIZATION_FULL:
        m_splitter.RestoreSplitterPos(0.5);
        m_subSplitter.RestoreSplitterPos(0.50);
        break;
    }
}

void CMainFrame::ApplyPaneVisibility(const bool restoreDuringScan)
{
    if (!restoreDuringScan && CWinDirStatModel::Get()->IsScanRunning())
    {
        ExpandFileTabbedView();
        return;
    }

    RestoreSplitterPositions();
    const bool showFileTypes = GetExtensionView()->IsShowTypes();
    const bool showVisualization = IsVisualizationShown();
    if (!showFileTypes && !showVisualization)
    {
        ExpandFileTabbedView();
        return;
    }
    if (!showFileTypes) MinimizeExtensionView();
    if (!showVisualization) MinimizeVisualizationPane();
}

void CMainFrame::RestoreVisualizationPane(const bool force)
{
    if (force) ShowVisualization(true);
    if (!IsVisualizationShown()) return;

    ApplyPaneVisibility();
    GetActiveVisualization()->RedrawWindow();
}

void CMainFrame::ShowVisualization(const bool show) const
{
    m_visualizationPane->ShowVisualization(show);
    COptions::ShowVisualization = show;
}

bool CMainFrame::IsVisualizationShown() const
{
    return m_visualizationPane->IsVisualizationShown();
}

CWinDirStatPane* CMainFrame::GetVisualizationPane() const
{
    return m_visualizationPane;
}

CWinDirStatPane* CMainFrame::GetActiveVisualization() const
{
    return m_visualizationPane->GetActiveView();
}

LRESULT CMainFrame::OnEnterSizeMove(WPARAM, LPARAM) const
{
    GetVisualizationPane()->SuspendRecalculationDrawing(true);
    return 0;
}

LRESULT CMainFrame::OnExitSizeMove(WPARAM, LPARAM) const
{
    GetVisualizationPane()->SuspendRecalculationDrawing(false);
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

    CFrameWnd::OnTimer(nIDEvent);
}

LRESULT CMainFrame::OnCallbackRequest(WPARAM, const LPARAM lParam)
{
    const auto & callback = *static_cast<std::function<void()>*>(std::bit_cast<LPVOID>(lParam));
    callback();
    return 0;
}
