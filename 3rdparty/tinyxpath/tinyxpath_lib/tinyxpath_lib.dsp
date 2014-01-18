# Microsoft Developer Studio Project File - Name="tinyxpath_lib" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=tinyxpath_lib - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "tinyxpath_lib.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "tinyxpath_lib.mak" CFG="tinyxpath_lib - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "tinyxpath_lib - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "tinyxpath_lib - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "tinyxpath_lib - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"Release\tinyxpath.lib"

!ELSEIF  "$(CFG)" == "tinyxpath_lib - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /FR /YX /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"Debug\tinyxpathd.lib"

!ENDIF 

# Begin Target

# Name "tinyxpath_lib - Win32 Release"
# Name "tinyxpath_lib - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Group "TinyXML"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\tinystr.cpp
# End Source File
# Begin Source File

SOURCE=..\tinystr.h
# End Source File
# Begin Source File

SOURCE=..\tinyxml.cpp
# End Source File
# Begin Source File

SOURCE=..\tinyxml.h
# End Source File
# Begin Source File

SOURCE=..\tinyxmlerror.cpp
# End Source File
# Begin Source File

SOURCE=..\tinyxmlparser.cpp
# End Source File
# End Group
# Begin Group "TinyXPath"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\action_store.cpp
# End Source File
# Begin Source File

SOURCE=..\action_store.h
# End Source File
# Begin Source File

SOURCE=..\byte_stream.h
# End Source File
# Begin Source File

SOURCE=..\lex_token.h
# End Source File
# Begin Source File

SOURCE=..\lex_util.cpp
# End Source File
# Begin Source File

SOURCE=..\lex_util.h
# End Source File
# Begin Source File

SOURCE=..\node_set.cpp
# End Source File
# Begin Source File

SOURCE=..\node_set.h
# End Source File
# Begin Source File

SOURCE=..\tinyxpath_conf.h
# End Source File
# Begin Source File

SOURCE=..\tokenlist.cpp
# End Source File
# Begin Source File

SOURCE=..\tokenlist.h
# End Source File
# Begin Source File

SOURCE=..\xml_util.cpp
# End Source File
# Begin Source File

SOURCE=..\xml_util.h
# End Source File
# Begin Source File

SOURCE=..\xpath_expression.cpp
# End Source File
# Begin Source File

SOURCE=..\xpath_expression.h
# End Source File
# Begin Source File

SOURCE=..\xpath_processor.cpp
# End Source File
# Begin Source File

SOURCE=..\xpath_processor.h
# End Source File
# Begin Source File

SOURCE=..\xpath_stack.cpp
# End Source File
# Begin Source File

SOURCE=..\xpath_stack.h
# End Source File
# Begin Source File

SOURCE=..\xpath_static.cpp
# End Source File
# Begin Source File

SOURCE=..\xpath_stream.cpp
# End Source File
# Begin Source File

SOURCE=..\xpath_stream.h
# End Source File
# Begin Source File

SOURCE=..\xpath_syntax.cpp
# End Source File
# Begin Source File

SOURCE=..\xpath_syntax.h
# End Source File
# End Group
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# End Group
# End Target
# End Project
