// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

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
    : MessageTarget(Localization::Lookup(IDS_WINDIRSTAT_SETTINGS))
{
}

bool CSettingsSheet::OnEraseBkgnd(CDC* pDC) const
{
    if (!DarkMode::IsDarkModeActive())
    {
        return CPropertySheet::OnEraseBkgnd(pDC);
    }

    // Paint the background with dark mode color
    const CRect rect = GetClientRect();
    pDC->FillSolidRect(rect, DarkMode::SystemColor(COLOR_WINDOW));

    return true;
}

bool CSettingsSheet::OnInitDialog()
{
    if (!CPropertySheet::OnInitDialog()) return false;

    Localization::UpdateDialogs(*this);
    Localization::UpdateTabControl(GetTabControl());
    DarkMode::AdjustControls(Handle());

    const int page = (m_initialPage >= 0) ? m_initialPage : static_cast<int>(COptions::ConfigPage);
    SelectPage(std::min(static_cast<int>(page), GetPageCount() - 1));
    GetTabControl().SetFocus();
    return false;
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
    COptions::ConfigPage = GetActivePageIndex();
    return CPropertySheet::OnCommand(wParam, lParam);
}

bool CSettingsSheet::ConfirmApply(const UINT command)
{
    m_restartAfterApply = false;
    if (!m_restartRequest || (command != IDOK && m_alreadyAsked)) return true;

    const int result = ShowMessageBox(*this, Localization::Lookup(IDS_RESTART_REQUEST),
        wds::strWinDirStat, MB_YESNOCANCEL);
    if (result == IDCANCEL) return false;
    m_alreadyAsked = result == IDNO;
    m_restartAfterApply = result == IDYES;
    return true;
}

void CSettingsSheet::OnApplied(const UINT command)
{
    m_restartApplication = m_restartAfterApply;
    if (m_restartApplication && command == ID_APPLY_NOW) RequestModalExit(IDOK);
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

void CWdsSplitterWnd::OnTrackingChanged(const bool tracking)
{
    // Keep the cached graph visible until a drag that resizes it finishes.
    if (auto* pane = CMainFrame::Get()->GetVisualizationPane(); IsChild(pane->Handle()))
        pane->SuspendRecalculationDrawing(tracking);
}

void CWdsSplitterWnd::StopTracking(const bool bAccept)
{
    CSplitterWnd::StopTracking(bAccept);
    if (!bAccept) return;

    const int currentPos = GetPaneSize(0);

    const CRect rcClient  = GetClientRect();
    const int   totalSize = IsVertical() ? rcClient.Width() : rcClient.Height();
    if (totalSize <= 0) return;

    const std::array paneSize = { currentPos, GetPaneSize(1) };
    for (const auto& [size, tracking] : std::views::zip(paneSize, m_paneTracking))
    {
        const auto& [onToggle, onMinimize] = tracking;
        if (!onToggle) continue;

        const bool isVisible = size > ScaleForDpi(COptions::MinimizeViewThreshold);
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
    const CRect rc = GetClientRect();
    if (const int size = static_cast<int>(pos * (IsVertical() ? rc.Width() : rc.Height())); size >= 0)
    {
        SetFirstPaneSize(size);
        UpdateLayout();
    }
}

void CWdsSplitterWnd::RestoreSplitterPos(const double posIfVirgin)
{
    SetSplitterPos(m_wasTrackedByUser ? *m_userSplitterPos : posIfVirgin);
    const CRect rc = GetClientRect();
    const int minimum = ScaleForDpi(COptions::MinimizeViewThreshold);
    const int size = static_cast<int>(m_splitterPos * (IsVertical() ? rc.Width() : rc.Height()));
    const bool collapsed = std::min(size, GetPaneSize(1)) <= minimum;
    if (m_wasTrackedByUser && collapsed) SetSplitterPos(posIfVirgin);
}

void CWdsSplitterWnd::OnSize(const UINT nType, const int cx, const int cy)
{
    if (const int size = static_cast<int>((IsVertical() ? cx : cy) * m_splitterPos); size > 0)
        SetFirstPaneSize(size);
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

int CPacmanControl::OnCreate(LPCREATESTRUCT /*lpCreateStruct*/)
{
    if (CallDefaultHandler() == -1)
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
    const CRect rc = GetClientRect();
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

void CMainFrame::OnSetFocus(WindowRef /*pOldWnd*/)
{
    CallDefaultHandler();
    if (HasFocus() && GetLogicalFocus() != LF_NONE)
    {
        MoveFocus(GetLogicalFocus());
    }
}

void CMainFrame::OnKeyDown(const UINT nChar, UINT /*nRepCnt*/, UINT /*nFlags*/)
{
    if (nChar == VK_TAB)
    {
        MoveFocus(LF_FILETREE);
        return;
    }
    CallDefaultHandler();
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
    // Directory structure may contain other volumes or internal loops
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

    CFileTreeControl::Get()->SortItems();
    CFileDupeControl::Get()->SortItems();
    CFileTopControl::Get()->SortItems();

    if (m_taskbarList)
    {
        m_taskbarList->SetProgressState(*this, m_taskbarButtonState = TBPF_NOPROGRESS);
    }

    DestroyProgress();
    CWinDirStatModel::Get()->SetScanTitlePrefix(wds::strEmpty);

    // Completion arrives as a sent message, so refresh toolbar commands without waiting for input
    CDirStatApp::Get()->OnIdle(0);
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
        CRect rc = m_wndStatusBar.GetPaneRect(CStatusBar::PaneId::Idle);
        rc.Deflate(m_wndStatusBar.ScaleForDpi(3), m_wndStatusBar.ScaleForDpi(4),
            m_wndStatusBar.ScaleForDpi(5), m_wndStatusBar.ScaleForDpi(4));
        rc.right = std::max(rc.left, rc.right);
        rc.bottom = std::max(rc.top, rc.bottom);
        m_progress.Create(WS_CHILD | WS_VISIBLE, rc, &m_wndStatusBar, ID_WDS_CONTROL);
        m_progress.ModifyStyle(WS_BORDER, 0);

        OnAppearanceChanged();
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
        const CRect rc = m_wndStatusBar.GetPaneRect(CStatusBar::PaneId::Idle);
        m_pacman.Create(nullptr, nullptr, WS_CHILD | WS_VISIBLE, rc, &m_wndStatusBar, ID_WDS_CONTROL);
        m_pacman.Start();
    }
}

void CMainFrame::DestroyProgress()
{
    if (IsWindow(m_progress))
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
    const auto cx = cdc.GetTextExtent(text).cx;
    m_wndStatusBar.SetPaneText(pane, text, std::max(static_cast<int>(cx), ScaleForDpi(minWidth)));
}

int CMainFrame::OnCreate(const LPCREATESTRUCT lpCreateStruct)
{
    if (CFrameWnd::OnCreate(lpCreateStruct) == -1)
    {
        return -1;
    }

    // Set up status pane and force initial field population
    m_wndStatusBar.Create(this);

    UpdatePaneText();

    m_wndToolBar.Create(this);
    m_wndToolBar.ModifyStyle(0, WS_CLIPCHILDREN);
    if (!m_watcherFilter.Create(WS_CHILD | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL,
        CRect(), &m_wndToolBar, ID_WATCHER_FILTER)) return -1;
    const auto filterHint = Localization::Lookup(IDS_WATCHER_FILTER);
    m_watcherFilter.SendNativeMessage(EM_SETCUEBANNER, true, filterHint.c_str());

    // Save the unscaled default button size before any SetMetrics call
    m_defaultButtonSize = m_wndToolBar.GetButtonSize();
    RebuildToolBar();

    // Show or hide status bar if requested
    if (!COptions::ShowStatusBar) m_wndStatusBar.ShowWindow(SW_HIDE);
    if (!COptions::ShowToolBar) m_wndToolBar.ShowWindow(SW_HIDE);

    // apply dark mode to main frame window
    DarkMode::AdjustControls(Handle());

    OnAppearanceChanged();

    return 0;
}

void CMainFrame::InitialShowWindow()
{
    const WINDOWPLACEMENT wpsetting = COptions::MainWindowPlacement;
    if (wpsetting.length != 0)
    {
        SetWindowPlacement(&wpsetting);
    }

    m_placementRestored = true;
    SetTimer(ID_WDS_CONTROL, 25);
}

void CMainFrame::InvokeInMessageThread(std::function<void()> callback) const
{
    if (m_ownerThreadId == GetCurrentThreadId()) callback();
    else Get()->SendMessage(WM_CALLBACKUI, 0, &callback);
}

void CMainFrame::OnClose()
{
    if (m_shuttingDown) return;
    CWaitCursor wc;

    // Mark process as shutting down
    m_shuttingDown = true;

    // Suspend the scan and wait for scan to complete
    CWinDirStatModel::Get()->StopScanningEngine(CWinDirStatModel::Abort);

    if (m_cleanupThread.joinable()) CWinApp::RunTaskWithUiUpdates([this] { m_cleanupThread.join(); });

    // Stop icon queue
    GetIconHandler()->StopAsyncShellInfoQueue();

    // It's too late to do this in OnDestroy() because the toolbar, if undocked,
    // is already destroyed in OnDestroy(). So we must save the toolbar state here
    // in OnClose().
    COptions::ShowToolBar = (m_wndToolBar.GetStyle() & WS_VISIBLE) != 0;
    COptions::ShowStatusBar = (m_wndStatusBar.GetStyle() & WS_VISIBLE) != 0;

    CallDefaultHandler();
}

void CMainFrame::OnDestroy()
{
    // Mark process as shutting down
    m_shuttingDown = true;

    // Force early cleanup of taskbar resources
    m_taskbarList.Release();

    SavePersistentAttributes();

    // Windows destroys child windows after WM_DESTROY returns.
    CFrameWnd::OnDestroy();
}

void CMainFrame::SavePersistentAttributes() const
{
    if (!m_placementRestored) return;

    // Save our window position
    WINDOWPLACEMENT wp = { .length = sizeof(wp) };
    GetWindowPlacement(&wp);
    COptions::MainWindowPlacement = wp;

    COptions::ShowToolBar = (m_wndToolBar.GetStyle() & WS_VISIBLE) != 0;
    COptions::ShowStatusBar = (m_wndStatusBar.GetStyle() & WS_VISIBLE) != 0;
    COptions::ShowFileTypes = GetExtensionView()->IsShowTypes();
    COptions::ShowVisualization = IsVisualizationShown();
}

void CMainFrame::OnEndSession(const bool ending) const
{
    if (ending) PersistedSetting::WritePersistedProperties();
}

void CMainFrame::PostNcDestroy()
{
    // Child controls have captured their final persistent state by this point.
    PersistedSetting::WritePersistedProperties();

    CFrameWnd::PostNcDestroy();
}

bool CMainFrame::OnCreateClient()
{
    // The splitters and views persist; RebuildLayout arranges the views for the selected layout.
    if (!m_splitter.CreateStatic(this, CSplitterWnd::Orientation::Horizontal) ||
        !m_subSplitter.CreateStatic(&m_splitter, CSplitterWnd::Orientation::Vertical)) return false;

    // Publish the views only once all of them exist.
    auto visualizationPane = std::make_unique<CVisualizationPane>();
    auto fileTabbedView = std::make_unique<CFileTabbedView>();
    auto extensionView = std::make_unique<CExtensionView>();
    for (CWnd* view : std::initializer_list<CWnd*>{ visualizationPane.get(), fileTabbedView.get(), extensionView.get() })
        if (!view->Create(nullptr, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS, CRect(), &m_splitter, 0))
            return false;
    m_visualizationPane = std::move(visualizationPane);
    m_fileTabbedView = std::move(fileTabbedView);
    m_extensionView = std::move(extensionView);

    GetExtensionView()->ShowTypes(COptions::ShowFileTypes);

    m_layoutPopup.Create(this);
    RebuildLayout();
    return true;
}

void CMainFrame::UpdateAllPanes(CWnd* sender, const MODEL_CHANGE change, CItem* item) const
{
    const std::array<CWinDirStatPane*, 3> panes{
        m_fileTabbedView.get(), m_extensionView.get(), m_visualizationPane.get()
    };
    for (CWinDirStatPane* pane : panes)
    {
        if (pane != nullptr && pane != sender)
        {
            pane->OnUpdate(sender, change, item);
        }
    }
}

void CMainFrame::UpdateFrameTitleForScan(const std::wstring_view scanName)
{
    SetDocumentTitle(scanName);
}

std::wstring CMainFrame::GetFullBaseTitle()
{
    return std::format(L"{}{}", GetAppTitle(),
        IsElevationActive() ? std::format(L" ({})", Localization::Lookup(IDS_ADMIN)) : wds::strEmpty);
}

bool CMainFrame::PreCreateWindow(CREATESTRUCT& cs)
{
    // seed initial Title bar text
    static std::wstring title = GetFullBaseTitle();
    cs.lpszName = title.c_str();
    cs.style |= WS_CLIPCHILDREN;

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

void CMainFrame::ExpandFileTabbedView()
{
    // Expand the file pane through its actual splitter ancestors.
    CWnd* pane = GetFileTabbedView();
    for (CWnd* parent = pane->GetParent<CWnd>(); parent != nullptr && parent->IsSplitterWindow();
        pane = parent, parent = pane->GetParent<CWnd>())
    {
        static_cast<CWdsSplitterWnd*>(parent)->SetSplitterPos(
            pane->GetDlgCtrlID() == CSplitterWnd::PaneId(0) ? 1.0 : 0.0);
    }
}

void CMainFrame::RestoreSplitterPositions()
{
    const auto& layout = CLayoutPopup::CurrentLayout();
    m_splitter.RestoreSplitterPos(layout.ratio);
    m_subSplitter.RestoreSplitterPos(layout.subRatio);
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
    // Collapse hidden panes in their owning splitter.
    const auto collapse = [](const CWinDirStatPane* pane)
    {
        pane->GetParent<CWdsSplitterWnd>()->SetSplitterPos(
            pane->GetDlgCtrlID() == CSplitterWnd::PaneId(0) ? 0.0 : 1.0);
    };
    if (!showFileTypes) collapse(GetExtensionView());
    if (!showVisualization) collapse(GetVisualizationPane());
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
    return m_visualizationPane.get();
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
    if (m_fontChangePending && ::IsWindowEnabled(m_hWnd)) OnFontSizeChanged(0, 0);

    if (m_cleanupQuery.valid() && WaitForSingleObject(m_cleanupThread.native_handle(), 0) == WAIT_OBJECT_0)
    {
        m_cleanupThread.join();
        try
        {
            const auto result = m_cleanupQuery.get();
            m_recycleBinItems = result[0];
            m_recycleBinBytes = result[1];
            m_shadowCopyCount = result[2];
            m_shadowCopyBytes = result[3];
            const auto [menu, position] = LocateNamedMenu(GetMenu(), Localization::Lookup(IDS_MENU_CLEANUP), false);
            if (menu != nullptr) UpdateCleanupMenu(menu, false);
        }
        catch (...) { VTRACE(L"Cleanup information query failed."); }
    }

    // Calculate UI updates that do not need to be processed frequently
    static unsigned int updateCounter = 0;
    const bool doInfrequentUpdate = updateCounter++ % 15 == 0;
    if (doInfrequentUpdate && !IsIconic())
    {
        // Update memory usage
        UpdatePaneText();
    }

    // UI updates that do need to be processed frequently
    if (!CWinDirStatModel::Get()->IsRootDone() && !IsScanSuspended())
    {
        // Update the visual progress at the bottom of the screen
        UpdateProgress();

        // Repaint progress independently of the less frequent row sort.
        if (doInfrequentUpdate) CFileTreeControl::Get()->SortItems();
        else CFileTreeControl::Get()->Invalidate(false);

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

    CWinDirStatModel::Get()->RunPendingHeapCleanup();

    CallDefaultHandler();
}

LRESULT CMainFrame::OnCallbackRequest(WPARAM, const LPARAM lParam)
{
    const auto & callback = *static_cast<std::function<void()>*>(std::bit_cast<LPVOID>(lParam));
    callback();
    return 0;
}

void CMainFrame::OnAppearanceChanged()
{
    const bool dark = DarkMode::IsDarkModeActive();
    if (m_progress.Handle() == nullptr) return;
    SetWindowTheme(m_progress.Handle(), dark ? L"" : nullptr, dark ? L"" : nullptr);
    m_progress.SetBkColor(dark ? DarkMode::Color(DarkMode::ColorRole::Frame) : CLR_DEFAULT);
}

bool CMainFrame::OnEraseBkgnd(CDC* dc)
{
    dc->FillSolidRect(GetClientRect(), DarkMode::Color(DarkMode::IsDarkModeActive() ?
        DarkMode::ColorRole::Window : DarkMode::ColorRole::Control));
    return true;
}
