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

#include "pch.h"
#include "SelectObject.h"
#include "SmartPointer.h"

//
// CIconHandler. Handles all shell information lookup.
//
class CIconHandler final
{
    static constexpr UINT WDS_SHGFI_DEFAULTS = SHGFI_USEFILEATTRIBUTES | SHGFI_SMALLICON | SHGFI_ICON | SHGFI_ADDOVERLAYS | SHGFI_SYSICONINDEX | SHGFI_OVERLAYINDEX;
    static constexpr auto MAX_ICON_THREADS = 4;

    std::mutex m_cachedIconMutex;
    std::unordered_map<int, HICON> m_cachedIcons;

public:
    CIconHandler() = default;
    ~CIconHandler();

    using IconLookup = std::tuple<CWdsListItem*, CWdsListControl*,
        std::wstring, DWORD, HICON*, std::wstring*>;

    void Initialize();
    void DoAsyncShellInfoLookup(IconLookup&& lookupInfo);
    void DrawIcon(const CDC* hdc, HICON image, const CPoint& pt, const CSize& sz);
    void ClearAsyncShellInfoQueue();
    void StopAsyncShellInfoQueue();

    HICON FetchShellIcon(const std::wstring& path, UINT flags = 0, DWORD attr = FILE_ATTRIBUTE_NORMAL, std::wstring* psTypeName = nullptr);

    BlockingQueue<IconLookup> m_lookupQueue = BlockingQueue<IconLookup>(false);

    HICON m_freeSpaceImage = nullptr;    // <Free Space>
    HICON m_unknownImage = nullptr;      // <Unknown>
    HICON m_hardlinksImage = nullptr;    // <Hardlinks>
    HICON m_dupesImage = nullptr;        // <Duplicates>
    HICON m_searchImage = nullptr;       // <Search>
    HICON m_largestImage = nullptr;      // <Largest>
    HICON m_emptyImage = nullptr;        // For items whose icon cannot be found
    HICON m_defaultFileImage = nullptr;  // Generic file icon while loading
    HICON m_defaultFolderImage = nullptr;// Generic folder icon while loading
    HICON m_junctionImage = nullptr;     // For normal junctions
    HICON m_symlinkImage = nullptr;      // For symbolic links
    HICON m_junctionProtected = nullptr; // For protected junctions
    HICON m_mountPointImage = nullptr;   // Mount point icon
    HICON m_myComputerImage = nullptr;   // My computer icon

    // Trivial getters
    HICON GetMyComputerImage() const { return m_myComputerImage; }
    HICON GetMountPointImage() const { return m_mountPointImage; }
    HICON GetJunctionImage() const { return m_junctionImage; }
    HICON GetSymbolicLinkImage() const { return m_symlinkImage; }
    HICON GetJunctionProtectedImage() const { return m_junctionProtected; }
    HICON GetFreeSpaceImage() const { return m_freeSpaceImage; }
    HICON GetUnknownImage() const { return m_unknownImage; }
    HICON GetEmptyImage() const { return m_emptyImage; }
    HICON GetHardlinksImage() const { return m_hardlinksImage; }
    HICON GetDupesImage() const { return m_dupesImage; }
    HICON GetSearchImage() const { return m_searchImage; }
    HICON GetLargestImage() const { return m_largestImage; }
};

namespace Icons
{
    using namespace Gdiplus;

    inline Color C(BYTE r, BYTE g, BYTE b, BYTE a = 255) { return Color(a, r, g, b); }
    inline Color Neutral() { return C(140, 140, 140); }

    void PaintDelete(Graphics& g);
    void PaintDeleteBin(Graphics& g);
    void PaintExplorerSelect(Graphics& g);
    void PaintOpenInConsole(Graphics& g);
    void PaintOpenSelected(Graphics& g);
    void PaintRefreshSelected(Graphics& g);
    void PaintProperties(Graphics& g);
    void PaintEditCopyClipboard(Graphics& g);
    void PaintFileSelect(Graphics& g);
    void PaintFilter(Graphics& g, bool active = false);
    void PaintHelp(Graphics& g);
    void PaintPause(Graphics& g);
    void PaintMagnifier(Graphics& g, bool plus);
    void PaintCharacter(Graphics& g, WCHAR ch, COLORREF clr,
                        bool bold = true, LPCWSTR fontName = wds::strFontSegoeUISymbol);

    std::function<void(Graphics&)> Char(WCHAR ch, COLORREF clr);

    HBITMAP MakeBitmap(int size, const std::function<void(Graphics&)>& painter);
    HICON   MakeIcon(int size, const std::function<void(Graphics&)>& painter);
    HICON   IconFromFontChar(WCHAR ch, COLORREF clr, bool bold = false,
                LPCWSTR fontName = wds::strFontCambriaMath, int iconSize = 0);

    template <auto Painter>
    HBITMAP Make(int size) { return MakeBitmap(size, Painter); }
}
