=============================================================================
WinDirStat - Windows Directory Statistics
(C) 2003-2005 Bernhard Seifert (bseifert@users.sourceforge.net)
(C) 2004-2006, 2008 Oliver Schneider (assarbad.net)
$Id$
=============================================================================

This project, made up of
- the program [executables]
	windirstat (English; default)

- resource DLLs [executables]
	wdsr0405 (Czech), wdsr0407 (German), wdsr040a (Spanish),
	wdsr040c (French), wdsr040e (Hungarian), wdsr0410 (Italian),
	wdsr0415 (Polish), wdsr0419 (Russian)

- helper/utility programs [executables]
	setup, linkcounter

- help files
	wdsh0407 (German), wdsh040e (Hungarian), wdsh0415 (Polish),
	wdshelp (English; default)

is distributed under the terms of the GPL v2 (executables+source)
respectively GNU FDL (help files).

See windirstat/res/license.txt and wdshelp/gnufdl.htm.

The tree-GIF was found in http://www.world-in-motion.de - I hope, I
don't violate any copyright (tell me).


=============================================================================
WinDirStat is a disk usage statistics viewer and cleanup tool for MS Windows
(all current variants). It shows disk, file and directory sizes in a treelist
as well as graphically in a treemap, much like KDirStat and Sequoiaview.


=============================================================================
If you make any changes or enhancements, find bugs or have feature requests,
please contact us. 

Bugs can be filed at https://sourceforge.net/p/windirstat/bugs/?source=navbar
until we switch to GitHub's issue tracker.

Note, however, that not all proposed features will be
taken into account as we want to keep WinDirStat what it is: a tool to view
disk usage statistics and to clean up your disk, simple and intuitive.


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
* Language is English by default; further translations can be added as
  resource DLLs,
* Online-Help,
* A little setup.exe which installs the files and shortcuts.


=============================================================================
This is a Microsoft Visual Studio.NET 2003 - Project.

Trying to backport this project was already done and failed. However, should
you succeed in doing so, please send us your changes.

Projects included in the workspace
----------------------------------
linkcounter	-> linkercounter.exe. Updates LINKCOUNT in common/version.h.
setup		-> setup.exe
wdsh0407	-> German Helpfile wdsh0407.chm
wdsh040e	-> Hungarian Helpfile wdsh040e.chm
wdsh0415	-> Polish Helpfile wdsh0415.chm
wdshelp		-> English Helpfile windirstat.chm
wdsr0405	-> Czech Resource DLL wdsr0407.dll
wdsr0407	-> German Resource DLL wdsr0407.dll
wdsr040a	-> Spanish Resource DLL wdsr040a.dll
wdsr040c	-> French Resource DLL wdsr040c.dll
wdsr040e	-> Hungarian Resource DLL wdsr040e.dll
wdsr0410	-> Italian Resource DLL wdsr0415.dll
wdsr0415	-> Polish Resource DLL wdsr0415.dll
wdsr0419	-> Russian Resource DLL wdsr0419.dll
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

* Remove the manifest and license.txt

* Translate the rc-File

=============================================================================

For the translators give credit by giving the language's name in the
respective language plus all the three-letter codes available for the
language in ISO 639.
Find the list of already included languages in the table below:

 ------------------------------------------------------------
 L-ID  | Language      | Native name  | 3-letter ISO 639 code
 (hex) | (english)     |              | (all available)
 ------------------------------------------------------------
 0405  | Czech         | Ce�tina      | CES/CZE
 0407  | German        | Deutsch      | DEU/GER
 0409  | English       | English      | ENG
 040a  | Spanish       | Espa�ol      | ESL/SPA
 040c  | French        | Fran�ais     | FRA/FRE
 040e  | Hungarian     | Magyar       | HUN
 0410  | Italian       | Italiano     | ITA
 0415  | Polish        | Polski       | POL
 0419  | Russian       | �������      | RUS
 ------------------------------------------------------------

To lookup further language codes look at:
-> http://www.w3.org/WAI/ER/IG/ert/iso639.htm

=============================================================================
testplan.txt may be useful for future releases.

---------------------------------- eof --------------------------------------
