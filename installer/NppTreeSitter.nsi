; installer/NppTreeSitter.nsi
; NSIS installer for NppTreeSitter — tree-sitter syntax highlighting for Notepad++.
;
; Cross-compilable from Linux: makensis installer/NppTreeSitter.nsi
;
; Expects the package tree to be at build/package/ relative to the repo root,
; or override with /DPACKAGE_DIR=<path> on the makensis command line.

!ifndef PACKAGE_DIR
    !define PACKAGE_DIR "..\build\package"
!endif

!ifndef VERSION
    !define VERSION "0.0.0"
!endif

; --- General ---
Name "NppTreeSitter ${VERSION}"
OutFile "..\build\NppTreeSitter-${VERSION}-setup.exe"
InstallDir "$PROGRAMFILES64\Notepad++"
InstallDirRegKey HKLM "Software\Notepad++" ""

RequestExecutionLevel admin
SetCompressor /SOLID lzma

; --- UI ---
!include "MUI2.nsh"

!define MUI_ABORTWARNING
!define MUI_ICON "${NSISDIR}\Contrib\Graphics\Icons\modern-install.ico"
!define MUI_UNICON "${NSISDIR}\Contrib\Graphics\Icons\modern-uninstall.ico"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

; --- Installation ---
Section "NppTreeSitter Plugin" SecPlugin
    SectionIn RO  ; required section

    ; Plugin DLL → <Notepad++>/plugins/NppTreeSitter/
    SetOutPath "$INSTDIR\plugins\NppTreeSitter"
    File "${PACKAGE_DIR}\plugins\NppTreeSitter\NppTreeSitter.dll"

    ; Config files -> %APPDATA%\Notepad++\plugins\config
    SetOutPath "$APPDATA\Notepad++\plugins\config"
    File "${PACKAGE_DIR}\config\NppTreeSitter.xml"

    ; Config dir -> %APPDATA%\Notepad++\plugins\config\NppTreeSitter
    SetOutPath "$APPDATA\Notepad++\plugins\config\NppTreeSitter"
    File "${PACKAGE_DIR}\config\NppTreeSitter\styles.conf"

    ; Grammar DLLs + query files
    ; We install everything under grammars/ recursively.
    SetOutPath "$APPDATA\Notepad++\plugins\config\NppTreeSitter\grammars"
    File /r "${PACKAGE_DIR}\config\NppTreeSitter\grammars\*.*"

    ; Write uninstaller
    WriteUninstaller "$INSTDIR\plugins\NppTreeSitter\uninstall-NppTreeSitter.exe"

    ; Registry keys for Add/Remove Programs
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\NppTreeSitter" \
        "DisplayName" "NppTreeSitter ${VERSION}"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\NppTreeSitter" \
        "UninstallString" '"$INSTDIR\plugins\NppTreeSitter\uninstall-NppTreeSitter.exe"'
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\NppTreeSitter" \
        "DisplayVersion" "${VERSION}"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\NppTreeSitter" \
        "Publisher" "Ben Mordaunt"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\NppTreeSitter" \
        "URLInfoAbout" "https://www.patreon.com/cw/BenMordaunt"
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\NppTreeSitter" \
        "NoModify" 1
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\NppTreeSitter" \
        "NoRepair" 1
SectionEnd

; --- Uninstallation ---
Section "Uninstall"
    ; Plugin DLL
    Delete "$INSTDIR\NppTreeSitter.dll"
    Delete "$INSTDIR\uninstall-NppTreeSitter.exe"
    RMDir "$INSTDIR"  ; only removes if empty (plugins\NppTreeSitter\)

    ; Config files
    RMDir /r "$APPDATA\Notepad++\plugins\config\NppTreeSitter"
    Delete "$APPDATA\Notepad++\plugins\config\NppTreeSitter.xml"

    ; Registry
    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\NppTreeSitter"
SectionEnd
