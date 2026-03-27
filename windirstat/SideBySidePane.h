// SideBySidePane.h
// Helper to toggle between stacked and side-by-side pane layouts.
// Minimal implementation that works with a standard CSplitterWnd.

#pragma once

#include <afxwin.h>

class CSideBySidePaneManager
{
public:
    // ctor expects a pointer to the splitter that hosts the three panes.
    explicit CSideBySidePaneManager(CSplitterWnd* splitter);

    // Switches between the default stacked layout and a side-by-side layout.
    // Returns true if the layout was changed, false on failure (e.g. null splitter).
    bool ToggleLayout();

    // Returns true if the current layout is side-by-side.
    bool IsSideBySide() const { return m_isSideBySide; }

private:
    CSplitterWnd* m_splitter;
    enum class LayoutMode {
        Stacked,
        SideBySide
    };
    LayoutMode m_isSideBySide;
    void ApplyStackedLayout();
    void ApplySideBySideLayout();
};