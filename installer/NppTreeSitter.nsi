; installer/NppTreeSitter.nsi
; NSIS installer for NppTreeSitter — tree-sitter syntax highlighting for Notepad++.
;
; This script is generated/augmented by the Makefile which appends grammar
; sections dynamically.  The static part below handles the plugin core,
; UI pages, and uninstaller.  Grammar sections are appended by
; `make installer` after the GRAMMAR_SECTIONS_MARKER line.

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

; Welcome page
!define MUI_WELCOMEPAGE_TITLE "NppTreeSitter ${VERSION}"
!define MUI_WELCOMEPAGE_TEXT "This wizard will install NppTreeSitter, a plugin that brings tree-sitter based syntax highlighting to Notepad++.$\r$\n$\r$\nYou will be able to select which language grammars to install.$\r$\n$\r$\nClick Next to continue."
!insertmacro MUI_PAGE_WELCOME

; License / support page (custom page)
Page custom SupportPageCreate

; Install directory
!insertmacro MUI_PAGE_DIRECTORY

; Component selection (grammars)
!define MUI_COMPONENTSPAGE_SMALLDESC
!insertmacro MUI_PAGE_COMPONENTS

; Install
!insertmacro MUI_PAGE_INSTFILES

; Finish
!define MUI_FINISHPAGE_LINK "Support this project on Patreon"
!define MUI_FINISHPAGE_LINK_LOCATION "https://www.patreon.com/cw/BenMordaunt"
!insertmacro MUI_PAGE_FINISH

; Uninstall pages
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

; --- Custom Support Page ---
Var SupportDialog
Var SupportLabel

Function SupportPageCreate
    nsDialogs::Create 1018
    Pop $SupportDialog
    ${If} $SupportDialog == error
        Abort
    ${EndIf}

    ${NSD_CreateLabel} 0 0 100% 60% "\
NppTreeSitter is free and open source software.$\r$\n\
$\r$\n\
It is built on tree-sitter, a fast incremental parsing library, and brings \
accurate syntax highlighting for languages not natively supported by \
Notepad++.$\r$\n\
$\r$\n\
If you find this plugin useful, please consider supporting its continued \
development. Your contributions help fund new grammar support, bug fixes, \
and improvements.$\r$\n\
$\r$\n\
Thank you for using NppTreeSitter!"
    Pop $SupportLabel

    ${NSD_CreateLink} 0 65% 100% 12u "Visit https://www.patreon.com/cw/BenMordaunt"
    Pop $0
    ${NSD_OnClick} $0 OnPatreonClick

    nsDialogs::Show
FunctionEnd

Function OnPatreonClick
    ExecShell "open" "https://www.patreon.com/cw/BenMordaunt"
FunctionEnd

; --- Core Plugin Section (required) ---
Section "NppTreeSitter Plugin (required)" SecPlugin
    SectionIn RO

    ; Plugin DLL
    SetOutPath "$INSTDIR\plugins\NppTreeSitter"
    File "${PACKAGE_DIR}\plugins\NppTreeSitter\NppTreeSitter.dll"

    ; NppTreeSitter.xml
    SetOutPath "$APPDATA\Notepad++\plugins\config"
    File "${PACKAGE_DIR}\config\NppTreeSitter.xml"

    ; styles.conf
    SetOutPath "$APPDATA\Notepad++\plugins\config\NppTreeSitter"
    File "${PACKAGE_DIR}\config\NppTreeSitter\styles.conf"

    ; Uninstaller
    WriteUninstaller "$INSTDIR\plugins\NppTreeSitter\uninstall-NppTreeSitter.exe"

    ; Add/Remove Programs
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

; --- Grammar Sections ---
; These are appended dynamically by the Makefile.
; Each grammar gets its own selectable section.
;GRAMMAR_SECTIONS_MARKER

; --- Section Descriptions ---
!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
    !insertmacro MUI_DESCRIPTION_TEXT ${SecPlugin} "The core NppTreeSitter plugin DLL and configuration files. Required."
    ;GRAMMAR_DESCRIPTIONS_MARKER
!insertmacro MUI_FUNCTION_DESCRIPTION_END

; --- Uninstallation ---
Section "Uninstall"
    ; Plugin
    Delete "$INSTDIR\NppTreeSitter.dll"
    Delete "$INSTDIR\uninstall-NppTreeSitter.exe"
    RMDir "$INSTDIR"

    ; Config
    RMDir /r "$APPDATA\Notepad++\plugins\config\NppTreeSitter"
    Delete "$APPDATA\Notepad++\plugins\config\NppTreeSitter.xml"

    ; Registry
    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\NppTreeSitter"
SectionEnd
