=============================================================================
WinDirStat - Windows Directory Statistics
(c) 2003 Bernhard Seifert (bseifert@users.sourceforge.net)
=============================================================================

This project, made up of
setup, wdsh0407, wdshelp, wdsr0407, and windirstat

is distributed under the terms of the GPL v2 (executables)
respectively GNU FDL (help files).

See Resource Files/license.txt and wdshelp/gnufdl.htm.

The tree-GIF was found in http://www.world-in-motion.de - I hope, I
don't violate any copyright (tell me).


=============================================================================
WinDirStat is a disk usage statistics viewer and cleanup tool for MS Windows 
(all current variants). It shows disk, file and directory sizes in a treelist 
as well as graphically in a treemap, much like KDirStat and Sequoiaview.


=============================================================================
WinDirStat is an application written in Visual C++ using MFC 7.0.
It runs on MS Windows (9x, NT, 2000, XP).

It shows where all your disk space is gone and helps you clean it up.

Design and many details are based on KDirStat (kdirstat.sourceforge.net).
WinDirStat is "a KDirStat re-programmed for MS Windows".

The directory tree is simultanously shown as a treelist and as a treemap.
One can effortlessly gain an impression of the proportions on the hard disk(s).

Major features:
* 3 views, Directory tree, Treemap and Extension list, coupled with each other,
* Built-in cleanup actions including Open, Delete, Show Properties,
* User defined cleanup actions (command line based),
* Language can be set to English or German; further translations can be added
   as resource DLLs,
* Online-Help,
* A little setup.exe which installs the files and shortcuts.



=============================================================================
This is a Microsoft Visual Studio.NET 2003 - Project.
Apart from CString::AppendFormat() I didn't use any MFC 7.0 specific features.
So this project can easily be ported to Visual Studio 6.0.

Projects included in the workspace
----------------------------------
linkcounter	-> linkercounter.exe. Updates LINKCOUNT in common/version.h.
setup		-> setup.exe
wdsh0407	-> German Helpfile wdsh0407.chm
wdshelp		-> English Helpfile windirstat.chm
wdsr0407	-> German Resource DLL wdsr0407.dll
wdsr040c	-> French Resource DLL wdsr040c.dll
windirstat	-> windirstat.exe (including English resources).

The Microsoft redistributable file shfolder.dll is also included.

I've commented 
- every source file
- every class
- every data member.

I haven't commented most member functions, maybe because my working English 
is not so good (I mostly would have duplicated the function name).


=============================================================================
How to create a resource dll.

* Determine the language id xx und sub-language id yy as defined in winnt.h.

* Create a new project wdsrxxyy.dll: Visual C++ Project
  - Win 32 Project - Dll - empty project.

* Copy windirstat.rc, resource.h and res/*.* into the
  wdsrxxyy-Folder respective the res-subfolder.

* In the linker options - advanced set Resource Only DLL to Yes.

* In Text Include 3 adjust the LANGUAGE

* Remove the manifest

* Translate the rc-File

* Note that the string IDS_RESOURCEVERSION (currently =  "Resource Version 3")
  must not be changed, otherwise the resource dll is not accepted
  by windirstat.exe.
  This version string may be changed in future versions of windirstat, if
  the resources must be changed, too.

=============================================================================

I'm distributing the ANSI variant. Feel free to build the UNICODE variant
(will not run on Windows 9x).


=============================================================================
testplan.txt may be useful for future releases.

---------------------------------- eof --------------------------------------
