<a name="windirstat-2.7.0"></a>
# WinDirStat 2.7.0 (Upcoming)

Enhancements
- Added Storage Analytics and cost optimization view
- Added permissions scanning and export capabilities
- Added window layout chooser for the main panes
- Added tools to remove empty folders and change folder dates to last change
- Added folder frames option to treemap visualization
- Added multilingual support for MSI installer
- Added file deletion warning before profile deletions
- Added xxHash (XXH3) as a selectable file hashing algorithm
- Added option to group unregistered file extensions
- Added shortcut to launch Microsoft Storage Settings
- Added extension search on double-click in extension list (thanks @harrytm)
- Added selected items count and statistics to status bar (thanks @harrytm)
- Improved column width autosizing performance in list and tree views
- Improved progress control rendering and layout behavior
- Improved multi-file operations and multi-selection properties dialog
- Improved CSV/JSON loading performance using buffered reads
- Added UTF-8 BOM support for CSV/JSON files
- Improved tab spacing in light mode
- Improved Italian translation (thanks @bovirus)

Bug Fixes
- Corrected accelerated scanning on Linux-created NTFS file systems
- Corrected potential crash when scanning root network shares
- Corrected potential shutdown hang or delay after a scan completes
- Corrected potential scan cancellation hang when worker thread is blocked
- Corrected progress bar behavior dropping from 100% back to 99% at scan completion
- Corrected layout alignment and spacing in setting dialogs
- Corrected localization language fallback logic regression (thanks @harrytm)
- Corrected various other minor issues

Miscellaneous
- Added MSIX bundle publishing support in WinGet
- Updated build script to default to the beta configuration after a short delay

<a name="windirstat-2.6.2"></a>
# WinDirStat 2.6.2

Enhancements
- Improved extension-checking performance during scan
- Improved icon-loading behavior to skip downloads for offline files
- Improved window flashing using non-interactive command line options
- Improved numeric display precision for percentages and sizes
- Improved security by preventing DLL sideloading
- Improved file select and document icons
- Improved Simplified Chinese translation
- Improved treemap rendering on low color depth Remote Desktop

Bug Fixes
- Corrected compression options not showing for a folder-based scan
- Corrected potential crash in file watcher component
- Corrected potential copy/paste path handling in the Select Drives dialog
- Corrected potential COM initialization to occur on the UI thread
- Corrected potential hang for unresponsive drive mappings

<a name="windirstat-2.6.1"></a>
# WinDirStat 2.6.1

Enhancements
- Improved gear, pause, filter, and refresh icon rendering
- Improved icons lookups on latent networks
- Improved treemap rendering performance

Bug Fixes
- Corrected some glyphs not rendering on Windows 7
- Corrected potential compressed size detection issue 
- Corrected potential reparse point enumeration issues
- Corrected file-based exclusions context menu option
- Corrected potential race condition during icon rendering
- Corrected minor handle leak in drive selection dialog
- Corrected potential crash when excluding reserved files
- Corrected Windows Store manifest generation issues
- Addressed folders not appearing in search results

<a name="windirstat-2.6.0"></a>
# WinDirStat 2.6.0

Enhancements
- Improved treemap navigation, zooming, and context-menu behavior
- Improved FileTreeView keyboard navigation and scroll-to-selection behavior
- Improved selection handling when sorting file-tree results
- Improved Select Drives dialog layout and local-drive display
- Added double-click scan behavior in the Select Drives dialog
- Added include filters and file age filtering
- Added filter exclusion context-menu actions
- Added pruning for previous-folder history
- Added larger toolbar icon support and runtime-generated toolbar icons
- Added Settings button to toolbar
- Improved toolbar rendering with Unicode glyph support
- Added selectable file hash algorithms
- Increased size and percentage display precision
- Changed subtree percentage display to size proportion terminology
- Added JSON support for saving/loading scan results and duplicate lists
- Expanded cleanup and system-tool handling
- Improved localization resources and multiple language updates
- Added installer option to add WinDirStat to the system PATH, disabled by default
- Removed OLE dependency to allow execution on Server Core
- Various performance, memory, and internal refactoring improvements

Bug Fixes
- Corrected custom folder icon resolution failures
- Corrected extension column autosizing issues
- Corrected extension calculations after refresh
- Correct legacy uninstall registry key removal
- Addressed selection regressions in owner-data tree/list handling
- Corrected drive handling when volume names or trailing slashes are missing
- Corrected cloud-duplication-detection warning behavior
- Corrected clipboard memory handling

Miscellaneous
- Added Microsoft Store MSIX packaging support
- Added Chocolatey release publishing automation
- Microsoft Windows Server Core now supported
- Microsoft Windows 7 supported again
- Microsoft Windows Server 2008 R2 supported again

## Breaking Changes
- Command line options for saving and loading have changed (see Wiki)

<a name="windirstat-2.5.0"></a>
# WinDirStat 2.5.0

Enhancements
- Added dark mode
- Added direct NTFS MFT scanning option
- Added hardlink tracking
- Added progress dialogs for long-running operations
- Added ability to search scan results
- Added ability to auto-search from extensions menu
- Added ability to move files
- Added ability to compute and display file hashes
- Added ability to save duplicate files to CSV file
- Added elevation eligibility detection and prompting
- Added new keyboard shortcuts
- Added previous folder tracking history
- Added option to launch programs and features applet
- Added option to optimize virtual hard disk files
- Added command line scanning options
- Added dism analyze option
- Added option to toggle Explorer context menu within application
- Added automatic copying of drive mapping when launched as admin
- Enhanced DPI awareness
- Changed sizes to use binary prefixes
- Changed treemap ability to show logical or physical sizes
- Changed file scan output to be sorted by path
- Changed column autosize to respect header width
- Changed volume display to show free, total, and percentages
- Changed compression option to recurse directories
- Changed CSV file output to be sorted by path
- Improved command-line parsing for target directory
- Improved delete and empty folder interface behaviors
- Improved various translations
- Added Swedish translation
- Added Japanese translation
- Added Turkish translation
- Various performance enhancements

Bug Fixes
- Corrected icon caching issues that could result in freezing
- Corrected exclusion case sensitivity selection for filtering
- Corrected extension colorization not working on file load
- Corrected highlight offset when not zoomed
- Corrected Explorer selection in treemap view
- Corrected potential crash when exiting program
- Corrected last-sibling node rendering
- Various other minor bug fixes

Miscellaneous
- Microsoft Windows 7 no longer supported
- Microsoft Windows Server 2008 R2 no longer supported
- ARM 32-Bit builds no longer supported
- CSV files created with previous versions will not be loadable

<a name="windirstat-2.2.2"></a>
# WinDirStat 2.2.2

## Enhancements
- Traditional Chinese language support (thanks @harryytm)
- Korean languages updates (thanks @VenusGirl)
- Gray-out user defined submenu if none are present
- Basic support for scanning \\\\?\\Volume{GUID} formatted paths
- Fallback deletion for hiberfil.sys 
- Various performance enhancements

## Bug Fixes
- Corrected hash entry inconsistencies in duplicate viewer
- Corrected tallying of physical sizes in duplicate viewer
- Corrected treemap zoomed view hit detection and highlighting
- Corrected treemap custom grid color not being applied

<a name="windirstat-2.2.1"></a>
# WinDirStat 2.2.1

## Enhancements
* Added "Largest Files" tab to the interface
* Increased the number of colorized extensions in TreeMap
* Added file extension information to duplicate list
* Numerous duplicate detection performance improvements
* Performance improvements when refreshing selected items
* Added Korean language support (thanks @VenusGirl)
* Added cleanup option to disable hibernate (hiberfil.sys)
* Added additional metadata to MSI installer
* Modified chocolatey installer to allow internalization
* Allow production / beta to update each other
* Improved legacy installer cleanup logic

## Bug Fixes
* Fixed missing overlays on some icons (e.g. .lnk files)
* Right-aligned numerical data in columns
* Allow WinDirStat.exe to accept folder as command line argument
* Addressed numerous potential hanging / crashing scenarios
* Addressed copy / paste not always working
* Addressed hiding toolbar and status bar setting persistence

<a name="windirstat-2.1.1"></a>
# WinDirStat 2.1.1

## Enhancements
* Ability to exclude folders by path
* Ability to exclude files by name
* Ability to exclude files by minimum file size
* Scans now stop quicker when requested during duplicate scan
* Slightly reduced executable size
* Better Norwegian translations (thanks @TilKenneth)
* Improved keyboard navigation on the file deletion dialog box
* Cleanup option to empty folder
* Improved file deletion progress indicator
* Display free space percentage next to volume label
* Other translations improvements (thanks @EricPossato, @tferrerm)
  
## Bug Fixes
* Addressed not being able to scan CSC directory
* Addressed not being to scan SUBST'd drives
* Addressed save/load files on Windows Server 2016 not working
* Addressed hover over treemap not showing filename properly
* Addressed not being able to scan in some Acronis folders

<a name="windirstat-2.0.3"></a>
# WinDirStat 2.0.3

## Enhancements
* Added status pane space usage summation for selected items
* Added attribute display character for sparse file (Z)

## Bug Fixes
* Addressed MSI installer not cleaning up old version properly
* Addressed potential hang when rendering tree icons
* Addressed behavior when calculating size for docker images
* Addressed size format not displaying after setting change 
* Addressed Norwegian language loading Dutch language
* Addressed Portuguese mistranslation (thanks @PedroBittarBarao)
* Addressed various typos in code comments (thanks @NathanBaulch)

<a name="windirstat-2.0.1"></a>
# WinDirStat 2.0.1

## Enhancements
* Multiple item selection
* Scanning performance enhancements
* Duplicate file finder based on file hashes
* Native 64-bit build now available
* Native ARM build now available
* Switched to MSI-based installer
* Menu shortcuts for popular native cleanup utilities
* Portable settings mode using WinDirStat.ini file
* Export scan results to CSV file
* Compress files using transparent compression capabilities
* Context menu option to display full Explorer context menu
* Context menu option to launch PowerShell
* Right-click explorer menu
* Toolbar icons enhanced
* Long file names are now supported
* Option to relaunch with elevated credentials
* Utilize backup / restore privileges to scan inaccessible files
* Pacman drawing enhancements
* Resolution scaling improvements
* Reparse point scanning exclusions
* Per-drive scanning multithreading
* Column to display file owner
* Column to distinguish logical versus physical allocation
* Built-in alternate languages translations
* Shell menu entry (legacy menu only)
* Numerous bug fixes
    
## Breaking Changes / Removed Features
* Non-default settings from 1.x will have to be set again
* Removed Files pseudo folder
* Removed option to email owner
* Removed help files
