# WinDirStat 2.1.0 (Upcoming)

## Enhancements
* Ability to exclude folders by path
* Ability to exclude files by name
* Ability to exclude files by minimum file size
* Scans now stop quicker when requested during duplicate scan
* Slightly reduced executable size
* Better Norwegian tranlsations (thanks @TilKenneth)
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
