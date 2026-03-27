// SideBySidePane.cpp
// Implementation of CSideBySidePaneManager.

#include "SideBySidePane.h"

CSideBySidePaneManager::CSideBySidePaneManager(CSplitterWnd* splitter)
    : m_splitter(splitter)
    , m_isSideBySide(false)
{
    // Ensure we start in the default stacked layout.
    if (m_splitter != nullptr)
    {
        ApplyStackedLayout();
    }
}

bool CSideBySidePaneManager::ToggleLayout()
{
    if (m_splitter == nullptr)
        return false;

    m_isSideBySide = !m_isSideBySide;
    if (m_isSideBySide)
        ApplySideBySideLayout();
    else
        ApplyStackedLayout();
    return true;
}

void CSideBySidePaneManager::ApplyStackedLayout()
{
    // Default layout: three rows, one column.
    // Row heights are distributed equally.
    const int totalHeight = m_splitter->GetClientRect().Height();
    const int rowHeight = totalHeight / 3;
    for (int i = 0; i < 3; ++i)
    {
        m_splitter->SetRowInfo(i, rowHeight, 0);
    }
    m_splitter->SetColumnInfo(0, m_splitter->GetClientRect().Width(), 0);
    m_splitter->RecalcLayout();
}

void CSideBySidePaneManager::ApplySideBySideLayout()
{
    // Side‑by‑side layout: one row, three columns.
    const int totalWidth = m_splitter->GetClientRect().Width();
    const int colWidth = totalWidth / 3;
    for (int i = 0; i < 3; ++i)
    {
        m_splitter->SetColumnInfo(i, colWidth, 0);
    }
    m_splitter->SetRowInfo(0, m_splitter->GetClientRect().Height(), 0);
    m_splitter->RecalcLayout();
}
