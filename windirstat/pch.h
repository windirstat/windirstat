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

// Exclude rarely-used stuff from Windows headers
#ifndef VC_EXTRALEAN
#define VC_EXTRALEAN
#endif

// Exclude unneeded MFC components
#define _AFX_NO_DAO_SUPPORT
#define _AFX_NO_CTL3D_SUPPORT
#define _ATL_NO_HOSTING
#define _ATL_NO_DOCHOSTUIHANDLER
#define _ATL_NO_UUIDOF

#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS  // some CStringW constructors will be explicit

// enables new GDI+ version
#define GDIPVER 0x0110

#include <afxwin.h>         // MFC Core
#include <afxext.h>         // MFC Extensions
#include <afxcontrolbars.h> // MFC support for ribbons and control bars

// Windows API headers
#include <VersionHelpers.h>
#include <psapi.h>
#include <winioctl.h>
#include <bcrypt.h>
#include <sal.h>
#include <wincrypt.h>
#include <sddl.h>
#include <winternl.h>
#include <powrprof.h>
#include <aclapi.h>
#include <uxtheme.h>
#include <vssym32.h>
#include <dwmapi.h>
#include <comdef.h>
#include <wbemidl.h>
#include <initguid.h>
#include <virtdisk.h>
#include <tlhelp32.h>
#include <fdi.h>

// STL headers
#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cstdint>
#include <cmath>
#include <condition_variable>
#include <execution>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <numeric>
#include <optional>
#include <queue>
#include <ranges>
#include <regex>
#include <set>
#include <shared_mutex>
#include <source_location>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

// Common WinDirStat headers
#include "resource.h"
#include "Tracer.h"
#include "DarkMode.h"
#include "Constants.h"
#include "HelpersTasks.h"
#include "HelpersInterface.h"
#include "BlockingQueue.h"
#include "Options.h"
#include "WinDirStat.h"
#include "DirStatDoc.h"
#include "Item.h"
#include "SmartPointer.h"
#include "Localization.h"
#include "MainFrame.h"
#include "MessageBoxDlg.h"
#include "LangStrings.h"
#include "SelectObject.h"
