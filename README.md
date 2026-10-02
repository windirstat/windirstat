# WinDirStat - Windows Directory Statistics

## Description

WinDirStat is a disk usage analyzer and cleanup assistant for Microsoft Windows. Scan drives, folders, network shares, or connected MTP devices, then explore disk usage through sortable file lists, file-type statistics, and interactive treemaps, sunbursts, or flame graphs.

Find large or duplicate files, inspect permissions, estimate storage costs, export reports, and run cleanup or deleted-file recovery tools. See the [website](https://windirstat.net/) for an overview and the [user guide](https://github.com/windirstat/windirstat/wiki) for usage details.

### Major features

* Scan multiple drives or folders together, with pause/resume, refresh, and accelerated local NTFS scanning when elevated
* Explore largest files, file types, logical versus physical size, hardlinks, and free/unknown space with configurable visualizations
* Search scanned results by name, size, file/folder type, or owner; filter scans by path, name, size, or age, with regular-expression support
* Detect duplicates using configurable hashes and cloud-file safeguards; optional sparse hashing labels sampled matches as **Probable duplicates**
* Inspect file and folder permission entries and estimate storage tier costs by file modification age
* Watch file-system changes, save/load scans as CSV or JSON, and export scan, duplicate, or permission reports from the command line
* [Recover deleted files](https://github.com/windirstat/windirstat/wiki/Recovering-Files) from local NTFS, exFAT, and FAT drives with administrator access; recovery depends on surviving metadata and file data
* Open, move, or delete files; run Windows cleanup tools, NTFS compression, hardlink deduplication, and custom cleanup commands
* Customize layouts, columns, colors, fonts, and toolbar size, with dark mode, portable settings, translations, and Explorer integration

For changes in recent versions, please check out [the change log](CHANGELOG.md).

### Getting started

1. Choose drives or **Individual Folders** in **Select Drives**. Use the add-folder button to scan several folders together. Connected MTP devices appear under **Individual Drives**.
2. Enable **Scan for duplicate files** only when needed; hashing adds file reads. For accelerated full-drive NTFS scans, use **File > Run Elevated**.
3. Sort **All Files** or open **Largest Files**, select an item to locate it in the visualization, and use `Ctrl+F` to search. Right-click column headers to choose details and use **View** to switch graphs.
4. Review items in Explorer before cleanup, then refresh affected results. Use **File > Save Results To CSV/JSON** to keep a scan snapshot.

For scripting, use `/SaveTo`, `/SaveDupesTo`, or `/SavePermsTo` followed by an output filename and scan targets. Save modes write a report and exit; `/LoadFrom` reopens a saved scan. See [command-line examples and export formats](https://github.com/windirstat/windirstat/wiki/Command-Line-and-CSV).

### Installation

The recommended way to install WinDirStat is with a package manager, which also makes future updates easier:

* Install from the [Microsoft Store](https://apps.microsoft.com/detail/9ph1gl95p3wf) for Store-managed installation and updates
* Install with `winget install -e --id WinDirStat.WinDirStat` (or use `winget upgrade` later)
* Install with `choco install windirstat` (or use `choco upgrade windirstat` later)
* Install with `scoop install extras/windirstat` (requires `scoop bucket add extras`)

If you prefer a manual installer, need a portable archive, or want to browse older versions and beta builds, use the [GitHub releases page](https://github.com/windirstat/windirstat/releases/). If you are not sure which file to choose, download the **64-bit MSI installer**.

| Download | Best for | What is inside |
| --- | --- | --- |
| [Microsoft Store app](https://apps.microsoft.com/detail/9ph1gl95p3wf) | Users who want one-click installation and Store-managed updates | Installs WinDirStat through the Microsoft Store app experience on supported Windows systems. |
| [WinDirStat-x64.msi](https://github.com/windirstat/windirstat/releases/latest/download/WinDirStat-x64.msi) | Most users on modern Intel or AMD 64-bit Windows PCs | Standard Windows installer for 64-bit systems. Adds WinDirStat to the Start menu and installs it like a normal app. |
| [WinDirStat-arm64.msi](https://github.com/windirstat/windirstat/releases/latest/download/WinDirStat-arm64.msi) | Windows on ARM devices, including newer Surface devices and other Snapdragon-based laptops | Standard Windows installer built for ARM64 Windows. |
| [WinDirStat-x86.msi](https://github.com/windirstat/windirstat/releases/latest/download/WinDirStat-x86.msi) | Older 32-bit Windows installations | Standard Windows installer for 32-bit systems. |
| [MSIX bundle](https://github.com/windirstat/windirstat/releases/latest) | Windows App Installer or Store-style deployment across different CPU types | If the release includes an `.msixbundle` asset, it can contain packages for multiple CPU types and Windows chooses the right package for your device. |
| [WinDirStat.zip](https://github.com/windirstat/windirstat/releases/latest/download/WinDirStat.zip) | Portable use, testing, or running without an installer | Zip archive containing the WinDirStat executables. Extract it first, then run the executable for your CPU type. |
| [WinDirStat.7z](https://github.com/windirstat/windirstat/releases/latest/download/WinDirStat.7z) | Portable use when you already have 7-Zip installed | Same kind of portable executable archive as the zip file, usually with a smaller download size. |

## Copyright / Licenses

* Copyright © WinDirStat Team ([windirstat.net](https://windirstat.net/))

The application itself is distributed under the terms of the [GPL v3 or later](windirstat/res/license.txt), but parts of the source code are also available under more lenient license terms.

The logo and all derivatives are available under the terms of the Creative
Commons license [CC BY 3.0](https://creativecommons.org/licenses/by/3.0/).

## Compatibility

WinDirStat 2.x has been developed for and tested on the following operating systems. It may work on older or newer operating systems, but those systems are not supported.

* Windows 7
* Windows 8
* Windows 8.1
* Windows 10
* Windows 11
* Windows Server 2008 R2
* Windows Server 2012
* Windows Server 2012 R2
* Windows Server 2016
* Windows Server 2019
* Windows Server 2022
* Windows Server 2025

## Resources

* A [website](https://windirstat.net/)
* The [user guide and reference](https://github.com/windirstat/windirstat/wiki)
* A [blog](https://blog.windirstat.net/)
* Twitter/X as [@windirstat](https://x.com/windirstat)
* SubReddit [r/WinDirStat](https://www.reddit.com/r/WinDirStat/)

Find a more up-to-date list of resources on the website and the blog at any point in time.

## Official Downloads and Malware Warning

WinDirStat's popularity has led to unofficial websites that copy the project's name, branding, or downloads. These sites are not operated by the WinDirStat team, may offer outdated or modified files, and may expose users to malware.

For your safety, install WinDirStat only through the Microsoft Store link and package managers listed above, the official [GitHub releases](https://github.com/windirstat/windirstat/releases/), or links from [windirstat.net](https://windirstat.net/). The team reports impersonation sites when possible, but takedowns are not always successful.

## Building

WinDirStat can be built with Visual Studio 2022 or later. A Visual Studio solution file can be loaded from `windirstat.sln`.

## Contributors

You can contribute by responding to issues, [developing](https://github.com/windirstat/windirstat/wiki/Developers) source code, or developing [translations](https://github.com/windirstat/windirstat/wiki/Contribute-Translation).

Thank you to everyone who has helped shape WinDirStat over the years.

<a href="https://github.com/windirstat/windirstat/graphs/contributors">
  <img src="https://contrib.rocks/image?repo=windirstat/windirstat&max=1000" alt="WinDirStat contributor tiles" />
</a>

For additional historical contributors, testers, and translators, please check out [the contributors page](CONTRIBUTORS.md).

## Logo

![WinDirStat logo](windirstat/logos/logo_256px.png)
