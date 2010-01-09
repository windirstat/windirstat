; Define the following symbols to get the intended functionality
; LANGUAGES_NSI
; GETVERSION_NSI
; WDSMAIN_NSI
!ifdef WDSMAIN_NSI
;--------------------------------
; Include Modern UI

  !include "MUI.nsh"

;--------------------------------
; General

  !include "${LANGANDVER}"
  !include "Sections.nsh"
  ; Name and file
  Name "${sVersionFull}"
  BrandingText "http://windirstat.sourceforge.net"
  OutFile "windirstat${sVersionFile}_setup.exe"
  SetDatablockOptimize on
  SetCompress force
  SetCompressor /FINAL lzma
  CRCCheck force
  XPStyle on
  LicenseForceSelection checkbox

  ; Default installation folder
  InstallDir "$PROGRAMFILES\WinDirStat"
  ; Get installation folder from registry if available
  !define APPKEY "Software\Seifert\WinDirStat"
  InstallDirRegKey HKCU "${APPKEY}" "InstDir"

;--------------------------------
; Global variables
  Var bIsNT
  Var dwMajorVersion
  Var dwMinorVersion
  Var sAnsiFilename
  !define SelectSection "!insertmacro SelectSection"
  !define UnselectSection "!insertmacro UnselectSection"
  !define RestoreSection "!insertmacro RestoreSection"
  !define SetSectionFlag "!insertmacro SetSectionFlag"
  !define ClearSectionFlag "!insertmacro ClearSectionFlag"
  !define SetSectionInInstType "!insertmacro SetSectionInInstType"
  !define ClearSectionInInstType "!insertmacro ClearSectionInInstType"
  !define SectionFlagIsSet "!insertmacro SectionFlagIsSet"
;--------------------------------
; Interface Settings

  !define MUI_ABORTWARNING
  !define MUI_COMPONENTSPAGE_SMALLDESC

;--------------------------------
; Pages

  !insertmacro MUI_PAGE_LICENSE "${Installer}\license.txt"
  !insertmacro MUI_PAGE_COMPONENTS
  !insertmacro MUI_PAGE_DIRECTORY
  !insertmacro MUI_PAGE_INSTFILES

; Define some values
  !define MUI_FINISHPAGE_LINK "Visit the WinDirStat website for news and hints."
  !define MUI_FINISHPAGE_LINK_LOCATION "http://windirstat.sourceforge.net/"
  !define MUI_FINISHPAGE_RUN "$INSTDIR\windirstat.exe"
  !define MUI_FINISHPAGE_NOREBOOTSUPPORT
; Insert finish page
  !insertmacro MUI_PAGE_FINISH

  !insertmacro MUI_UNPAGE_CONFIRM
  !insertmacro MUI_UNPAGE_INSTFILES

;--------------------------------
;Languages

  !insertmacro MUI_LANGUAGE "English"

;--------------------------------
; Version Information

  VIProductVersion "${sVersion_Ansi}"
  VIAddVersionKey /LANG=${LANG_ENGLISH} "ProductName" "WinDirStat"
  VIAddVersionKey /LANG=${LANG_ENGLISH} "Comments" "This release contains both, Unicode and ANSI version of WinDirStat"
  VIAddVersionKey /LANG=${LANG_ENGLISH} "CompanyName" "WDS Team"
  VIAddVersionKey /LANG=${LANG_ENGLISH} "LegalCopyright" "© 2003-2005 WDS Team"
  VIAddVersionKey /LANG=${LANG_ENGLISH} "FileDescription" "${sVersionFull}"
  VIAddVersionKey /LANG=${LANG_ENGLISH} "FileVersion" "${sVersionS}"
  VIAddVersionKey /LANG=${LANG_ENGLISH} "InternalName" "WDS Setup"
  VIAddVersionKey /LANG=${LANG_ENGLISH} "OriginalFilename" "WinDirStat${sVersionFile}_setup.exe"
  VIAddVersionKey /LANG=${LANG_ENGLISH} "SpecialBuild" "This setup is still a beta version!"

;--------------------------------
; Reserve Files

  ;These files should be inserted before other files in the data block
  ;Keep these lines before any File command
  ;Only for solid compression (by default, solid compression is enabled for BZIP2 and LZMA)

;--------------------------------
; Installer Sections

InstType "Full installation"
InstType "Recommended only (all languages)"
InstType "Recommended only (without languages)"

Icon "${Installer}\icon1.ico"
SubSection /e "Core components (English incl. help)"
  ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
  ; Check the major version against 4, if equal or less copy the shfolder.dll
  ; which is required on Windows NT 4.0 and 9x/Me only.
  ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
  Section "-Required"
    SectionIn RO
    SetOutPath $INSTDIR
    IntCmpU $dwMajorVersion 4 0 0 +3
    File ${Installer}\shfolder.dll
    Goto +2
    DetailPrint "Skipping shfolder.dll (Windows $dwMajorVersion.$dwMinorVersion)"
    File ${Installer}\windirstat.chm
    ; Store installation folder
    WriteRegStr HKCU "Software\Seifert\WinDirStat" "InstDir" $INSTDIR
    Call CreateUninstallEntry
  SectionEnd

  ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
  ; On Windows 9x/Me you will only get to select the ANSI version of WDS.
  ; The EXE file is being copied as "windirstat.exe" then.
  ;
  ; On the NT platform you *may* select ANSI additionally (Unicode is
  ; preselected and cannot be unchecked) in which case it is copied as
  ; "windirstatA.exe"
  ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
  Section "ANSI (95/98/Me)" Ansi
    SectionIn 1
    StrCmp $bIsNT "0" 0 +3
    ; If not running on NT, use windirstat.exe as filename
    StrCpy $sAnsiFilename "windirstat.exe"
    Goto +2
    StrCpy $sAnsiFilename "windirstatA.exe"
    ; ... else use windirstatA.exe
    File /oname=$INSTDIR\$sAnsiFilename ${Installer}\windirstatA.exe
  SectionEnd

  ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
  ; This section is only of importance on the NT platform, else the file is
  ; being skipped completely.
  ;
  ; On NT you cannot uncheck Unicode, so it will be installed anyway.
  ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
  Section "Unicode (NT/2K/XP/2003/...)" Unicode
    SectionIn 1
    ; If not running on NT, skip this file
    StrCmp $bIsNT "0" +2
    File /oname=$INSTDIR\windirstat.exe ${Installer}\windirstatU.exe
  SectionEnd

  ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
  ; This will create the following shortcuts under program in the start menu:
  ; -> WinDirStat\WinDirStat.lnk
  ; (OR, if running on NT platform and additionally ANSI was selected)
  ; -> WinDirStat\WinDirStat (Unicode).lnk
  ;    WinDirStat\WinDirStat (ANSI).lnk
  ; ** WinDirStat\Uninstall WinDirStat.lnk
  ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
  Section "Create a program group in startmenu" Startmenu
    SectionIn 1 2 3
    CreateDirectory "$SMPROGRAMS\WinDirStat"
    StrCmp $bIsNT "0" 0 +3
    CreateShortCut "$SMPROGRAMS\WinDirStat\WinDirStat.lnk" "$INSTDIR\windirstat.exe"
    Goto NotNT
    ; If ANSI was selected on NT
    ${SectionFlagIsSet} ${Ansi} ${SF_SELECTED} 0 OnlyUnicode
    CreateShortCut "$SMPROGRAMS\WinDirStat\WinDirStat (ANSI).lnk" "$INSTDIR\windirstatA.exe"
    CreateShortCut "$SMPROGRAMS\WinDirStat\WinDirStat (Unicode).lnk" "$INSTDIR\windirstat.exe"
    Goto NotNT
    ; If only Unicode was selected on NT
  OnlyUnicode:
    CreateShortCut "$SMPROGRAMS\WinDirStat\WinDirStat.lnk" "$INSTDIR\windirstat.exe"
  NotNT:
    CreateShortCut "$SMPROGRAMS\WinDirStat\Uninstall WinDirStat.lnk" "$INSTDIR\Uninstall.exe"
    CreateShortCut "$SMPROGRAMS\WinDirStat\Help (English).lnk" "$INSTDIR\windirstat.chm"
  SectionEnd

  ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
  ; This will create a single shortcut to the recommended program version on
  ; the users desktop.
  ; ** WinDirStat.lnk
  ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
  Section "Create shortcut on the desktop" Desktop
    SectionIn 1 2 3
    CreateShortCut "$DESKTOP\WinDirStat.lnk" "$INSTDIR\windirstat.exe"
  SectionEnd
SubSectionEnd

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Automatically created list of supported languages and their names.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
SubSection /e "Languages"
  !define LANGSECTIONS
  !include "${LANGTEMP}"
  !undef LANGSECTIONS
SubSectionEnd

;--------------------------------
;Installer Functions

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; This function is a callback being called upon initialization. It checks the
; OS versionon which we run and decides about the available options.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
Function .onInit
  Call CheckOS
  StrCmp $bIsNT "0" 0 IsNT
; Hide the unicode section
  SectionSetText ${Unicode} ""
; Select ANSI and make if read-only
  ${SelectSection} ${Ansi}
  ${SetSectionFlag} ${Ansi} ${SF_BOLD}
  ${SetSectionFlag} ${Ansi} ${SF_RO}
  ${SetSectionInInstType} ${Ansi} ${INSTTYPE_2}
  ${SetSectionInInstType} ${Ansi} ${INSTTYPE_3}
  ${ClearSectionInInstType} ${Unicode} ${INSTTYPE_1}
  ${ClearSectionInInstType} ${Unicode} ${INSTTYPE_2}
  ${ClearSectionInInstType} ${Unicode} ${INSTTYPE_3}
  SectionSetSize ${Unicode} 0
IsNT:
; Make the unicode section checked and bold
  ${SelectSection} ${Unicode}
  ${SetSectionFlag} ${Unicode} ${SF_BOLD}
  ${SetSectionFlag} ${Unicode} ${SF_RO}
  ${SetSectionInInstType} ${Unicode} ${INSTTYPE_2}
  ${SetSectionInInstType} ${Unicode} ${INSTTYPE_3}
; Set default install type to "recommended"
  SetCurInstType 2
; Now check current thread locale
  System::Call "kernel32::GetThreadLocale() i .r0"
  !define LANGCURRLANG
  !include "${LANGTEMP}"
  !undef LANGCURRLANG
FunctionEnd

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Check the major and minor version number and check for NT
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
Function CheckOS
  Push $0
  Push $1
  System::Call 'kernel32::GetVersion() i .r0'
  IntOp $1 $0 & 0xFFFF
  IntOp $dwMajorVersion $1 & 0xFF
  IntOp $dwMinorVersion $1 >> 8
  IntOp $0 $0 & 0x80000000
  IntCmpU $0 0 IsNT Is9x Is9x
Is9x:
  StrCpy $bIsNT "0"
  Goto AfterOsCheck
IsNT:
  StrCpy $bIsNT "1"
AfterOsCheck:
  Pop $1
  Pop $0
FunctionEnd

Function CreateUninstallEntry
  ; Create the uninstaller
  WriteUninstaller "$INSTDIR\Uninstall.exe"
  WriteRegExpandStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\WinDirStat" "UninstallString" '"$INSTDIR\Uninstall.exe"'
  WriteRegExpandStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\WinDirStat" "InstallLocation" "$INSTDIR"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\WinDirStat" "DisplayName" "${sVersionFull}"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\WinDirStat" "DisplayIcon" "$INSTDIR\windirstat.exe,0"
  WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\WinDirStat" "dwVersionMajor" "${dVersionMajor}"
  WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\WinDirStat" "dwVersionMinor" "${dVersionMinor}"
  WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\WinDirStat" "dwVersionRev" "${dVersionRev}"
  WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\WinDirStat" "dwVersionBuild" "${dVersionBuild_Ansi}"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\WinDirStat" "URLInfoAbout" "http://windirstat.sourceforge.net/"
  WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\WinDirStat" "NoModify" "1"
  WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\WinDirStat" "NoRepair" "1"
FunctionEnd
; -----------------------------------------------------------------------------

;--------------------------------
;Descriptions

; USE A LANGUAGE STRING IF YOU WANT YOUR DESCRIPTIONS TO BE LANGAUGE SPECIFIC

; Assign descriptions to sections
!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${Ansi} "Installs the ANSI version of WinDirStat, compatible with any Windows version. [Version ${sVersion_Ansi}]"
  !insertmacro MUI_DESCRIPTION_TEXT ${Unicode} "Installs the Unicode version of WinDirStat, compatible with Windows NT/2000/XP/2003. [Version ${sVersion_Unicode}]"
  !insertmacro MUI_DESCRIPTION_TEXT ${Startmenu} "Creates a program group in your startmenu."
  !insertmacro MUI_DESCRIPTION_TEXT ${Desktop} "Creates a shortcut on the desktop which points to the component shown in bold."
  !define DESCRIBING
  !include "${LANGTEMP}"
  !undef DESCRIBING
!insertmacro MUI_FUNCTION_DESCRIPTION_END

;--------------------------------
;Uninstaller Section
UninstallIcon "${Installer}\icon2.ico"

Section "Uninstall"

; Delete the WDS files
  Delete "$INSTDIR\windirstat.exe"
  IfFileExists "$INSTDIR\windirstatA.exe" 0 +2
  Delete "$INSTDIR\windirstatA.exe"
  IfFileExists "$INSTDIR\shfolder.dll" 0 +2
  Delete "$INSTDIR\shfolder.dll"
  Delete "$INSTDIR\windirstat.chm"
; Delete additional languag files. Only these which had been included in the installer!
  !define DELLANGFILES
  !include "${LANGTEMP}"
  !undef DELLANGFILES
; Remove the WDS install directory, if empty
  RMDir /r "$INSTDIR"

; Delete program group entries
  IfFileExists "$SMPROGRAMS\WinDirStat\WinDirStat.lnk" 0 +2
  Delete "$SMPROGRAMS\WinDirStat\WinDirStat.lnk"
  IfFileExists "$SMPROGRAMS\WinDirStat\WinDirStat (ANSI).lnk" 0 +2
  Delete "$SMPROGRAMS\WinDirStat\WinDirStat (ANSI).lnk"
  IfFileExists "$SMPROGRAMS\WinDirStat\WinDirStat (Unicode).lnk" 0 +2
  Delete "$SMPROGRAMS\WinDirStat\WinDirStat (Unicode).lnk"

  Delete "$SMPROGRAMS\WinDirStat\Uninstall WinDirStat.lnk"
; Remove the program group, if empty
  RMDir /r "$SMPROGRAMS\WinDirStat"

; Remove desktop link
  Delete "$DESKTOP\WinDirStat.lnk"

; Remove the value for the install directory
  DeleteRegValue HKCU "Software\Seifert\WinDirStat" "InstDir"
; Delete the WDS subkey
  DeleteRegKey /ifempty HKCU "Software\Seifert\WinDirStat"
; Delete the parent key as well if WDS was the only app in this subkey
  DeleteRegKey /ifempty HKCU "Software\Seifert"

  Call un.CreateUninstallEntry
SectionEnd

;--------------------------------
;Uninstaller Functions

Function un.onInit
  Call un.CheckOS
FunctionEnd

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Check the major and minor version number and check for NT
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
Function un.CheckOS
  Push $0
  Push $1
  System::Call 'kernel32::GetVersion() i .r0'
  IntOp $1 $0 & 0xFFFF
  IntOp $dwMajorVersion $1 & 0xFF
  IntOp $dwMinorVersion $1 >> 8
  IntOp $0 $0 & 0x80000000
  IntCmpU $0 0 IsNT Is9x Is9x
Is9x:
  StrCpy $bIsNT "0"
  Goto AfterOsCheck
IsNT:
  StrCpy $bIsNT "1"
AfterOsCheck:
  Pop $1
  Pop $0
FunctionEnd

Function un.CreateUninstallEntry
; Delete the uninstaller
  Delete "$INSTDIR\Uninstall.exe"
  DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\WinDirStat"
  DeleteRegKey HKCU "${APPKEY}"
FunctionEnd
; -----------------------------------------------------------------------------
!endif ; WDSMAIN_NSI

!ifdef LANGUAGES_NSI
OutFile "${InstallerOut}"

Function .onInit

; Write it to a !define for use in main script
  FileOpen $R0 "$EXEDIR\${LangOut}" a
  FileSeek $R0 0 END
  Push $0 ;save
  Call LangNameEnglish
  Pop $0
  FileWrite $R0 '!define LangEngName${LANGID} "'
  FileWrite $R0 '$0"$\r$\n'
  Call LangNameNative
  Pop $0
  FileWrite $R0 '!define LangNatName${LANGID} "'
  FileWrite $R0 '$0"$\r$\n'
  Call LangNameShort
  Pop $0
  FileWrite $R0 '!define LangShoName${LANGID} "'
  FileWrite $R0 '$0"$\r$\n'
  Pop $0  ; restore
  FileClose $R0

  Abort
FunctionEnd

Function LangNameEnglish
  Push $0   ; push the old value of variable $0
  System::Alloc "${NSIS_MAX_STRLEN}"
  Pop $0    ; get the return value of System::Alloc
  System::Call "Kernel32::GetLocaleInfo(i,i,t,i)i(${LCID},0x00001001,.r0,${NSIS_MAX_STRLEN})i"
  Exch $0   ; restore old value of variable $0
FunctionEnd

Function LangNameNative
  Push $0   ; push the old value of variable $0
  System::Alloc "${NSIS_MAX_STRLEN}"
  Pop $0    ; get the return value of System::Alloc
  System::Call "Kernel32::GetLocaleInfo(i,i,t,i)i(${LCID},0x00000004,.r0,${NSIS_MAX_STRLEN})i"
  Exch $0   ; restore old value of variable $0
FunctionEnd

Function LangNameShort
  Push $0   ; push the old value of variable $0
  System::Alloc "${NSIS_MAX_STRLEN}"
  Pop $0    ; get the return value of System::Alloc
  System::Call "Kernel32::GetLocaleInfo(i,i,t,i)i(${LCID},0x00000003,.r0,${NSIS_MAX_STRLEN})i"
  Exch $0   ; restore old value of variable $0
FunctionEnd

Section
SectionEnd
!endif ; LANGUAGES_NSI

!ifdef GETVERSION_NSI
OutFile "${InstallerOut}"

Function .onInit 
; Get file version 
 GetDllVersion "${VersionOfFile}" $R0 $R1 
  IntOp $R2 $R0 / 0x00010000 
  IntOp $R3 $R0 & 0x0000FFFF 
  IntOp $R4 $R1 / 0x00010000 
  IntOp $R5 $R1 & 0x0000FFFF 
  StrCpy $R1 "$R2.$R3.$R4.$R5" 
 
; Write it to a !define for use in main script 
  FileOpen $R0 "$EXEDIR\${VersionOut}" w 
  FileWrite $R0 '$R1'
  FileClose $R0

  Abort
FunctionEnd

Section
SectionEnd
!endif ; GETVERSION_NSI

