// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#pragma once

#define NOMINMAX

#define _ATL_NO_HOSTING
#define _ATL_NO_DOCHOSTUIHANDLER
#define _ATL_NO_UUIDOF

// Enable the current GDI+ API surface.
#define GDIPVER 0x0110

// Windows and ATL headers
#include <windows.h>
#include <windowsx.h>
#include <VersionHelpers.h>
#include <aclapi.h>
#include <atlbase.h>
#include <bcrypt.h>
#include <commctrl.h>
#include <commdlg.h>
#include <comdef.h>
#include <dwmapi.h>
#include <fdi.h>
#include <gdiplus.h>
#include <olectl.h>
#include <powrprof.h>
#include <PortableDeviceApi.h>
#include <PortableDevice.h>
#include <propkey.h>
#include <psapi.h>
#include <richedit.h>
#include <sal.h>
#include <sddl.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <tlhelp32.h>
#include <uxtheme.h>
#include <vssym32.h>
#include <wbemidl.h>
#include <initguid.h>
#include <virtdisk.h>
#include <wincrypt.h>
#include <winioctl.h>
#include <winternl.h>

// Standard library headers
#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cassert>
#include <cctype>
#include <cerrno>
#include <chrono>
#include <climits>
#include <cmath>
#include <concepts>
#include <condition_variable>
#include <cstdarg>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <cwctype>
#include <exception>
#include <execution>
#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <future>
#include <initializer_list>
#include <iostream>
#include <limits>
#include <list>
#include <malloc.h>
#include <map>
#include <memory>
#include <mutex>
#include <numeric>
#include <numbers>
#include <optional>
#include <queue>
#include <ranges>
#include <regex>
#include <set>
#include <shared_mutex>
#include <source_location>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "cabinet.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "msimg32.lib")
#pragma comment(lib, "powrprof.lib")
#pragma comment(lib, "PortableDeviceGuids.lib")
#pragma comment(lib, "propsys.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "version.lib")
#pragma comment(lib, "virtdisk.lib")
#pragma comment(lib, "wbemuuid.lib")

#include "UiFramework.h"

// Third-party libraries
#include <contrib/xxhash/xxhash.h>

// Common WinDirStat headers
#include "Tracer.h"
#include "DarkMode.h"
#include "Constants.h"
#include "HelpersTasks.h"
#include "HelpersInterface.h"
#include "BlockingQueue.h"
#include "Options.h"
#include "WinDirStat.h"
#include "WinDirStatModel.h"
#include "Item.h"
#include "SmartPointer.h"
#include "Localization.h"
#include "MainFrame.h"
#include "MessageBoxDlg.h"
#include "LangStrings.h"
