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

// Win32++ headers
// NOTE: These headers are expected to be provided by the vendored Win32++ SDK
// under third_party/win32xx/include (configured in windirstat.vcxproj).
#define GDIPVER 0x0110

#include <wxx_appcore.h>
#include <wxx_cstring.h>
#include <wxx_controls.h>
#include <wxx_dialog.h>
#include <wxx_docview.h>
#include <wxx_splitter.h>
#include <wxx_frame.h>
#include <wxx_gdi.h>
#include <wxx_listview.h>
#include <wxx_menu.h>
#include <wxx_propertysheet.h>
#include <wxx_tab.h>
#include <wxx_treeview.h>
#include <wxx_wincore.h>

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
#include <stdexcept>
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
