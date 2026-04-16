; installer/NppTreeSitter.nsi
; NSIS installer for NppTreeSitter.
; Grammar sections are injected by installer/generate_nsi.py at the markers.

!ifndef PACKAGE_DIR
    !define PACKAGE_DIR "..\build\package"
!endif

!ifndef VERSION
    !define VERSION "0.0.0"
!endif

Name "NppTreeSitter ${VERSION}"
OutFile "..\build\NppTreeSitter-${VERSION}-setup.exe"
InstallDir "$PROGRAMFILES64\Notepad++"
InstallDirRegKey HKLM "Software\Notepad++" ""

RequestExecutionLevel admin
SetCompressor /SOLID lzma

; --- Includes ---
!include "MUI2.nsh"
!include "nsDialogs.nsh"
!include "LogicLib.nsh"

!define MUI_ABORTWARNING
!define MUI_ICON "${NSISDIR}\Contrib\Graphics\Icons\modern-install.ico"
!define MUI_UNICON "${NSISDIR}\Contrib\Graphics\Icons\modern-uninstall.ico"

; --- Pages ---
!define MUI_WELCOMEPAGE_TITLE "NppTreeSitter ${VERSION}"
!define MUI_WELCOMEPAGE_TEXT "This wizard will install NppTreeSitter, a plugin \
that brings tree-sitter based syntax highlighting to Notepad++.$\r$\n$\r$\n\
You will be able to select which language grammars to install.$\r$\n$\r$\n\
Click Next to continue."
!insertmacro MUI_PAGE_WELCOME

; Custom support/donate page
Page custom SupportPageCreate

!insertmacro MUI_PAGE_DIRECTORY

!define MUI_COMPONENTSPAGE_SMALLDESC
!insertmacro MUI_PAGE_COMPONENTS

!insertmacro MUI_PAGE_INSTFILES

!define MUI_FINISHPAGE_LINK "Support this project on Patreon"
!define MUI_FINISHPAGE_LINK_LOCATION "https://www.patreon.com/cw/BenMordaunt"
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

; --- Support Page ---
Var SupportDialog

Function SupportPageCreate
    nsDialogs::Create 1018
    Pop $SupportDialog
    ${If} $SupportDialog == error
        Abort
    ${EndIf}

    ${NSD_CreateLabel} 0 0 100% 55% "\
NppTreeSitter is free and open source software, built on tree-sitter.$\r$\n\
$\r$\n\
It brings accurate, fast syntax highlighting for languages that are not \
natively supported by Notepad++.$\r$\n\
$\r$\n\
If you find this plugin useful, please consider supporting its continued \
development. Your contributions help fund new grammar support, bug fixes, \
and improvements.$\r$\n\
$\r$\n\
Thank you for using NppTreeSitter!"
    Pop $0

    ${NSD_CreateLink} 0 60% 100% 12u "https://www.patreon.com/cw/BenMordaunt"
    Pop $0
    ${NSD_OnClick} $0 OnPatreonClick

    nsDialogs::Show
FunctionEnd

Function OnPatreonClick
    ExecShell "open" "https://www.patreon.com/cw/BenMordaunt"
FunctionEnd

; --- Check if Notepad++ is running ---
Function .onInit
    FindWindow $0 "Notepad++"
    ${If} $0 != 0
        MessageBox MB_OKCANCEL|MB_ICONEXCLAMATION \
            "Notepad++ is currently running.$\r$\n$\r$\n\
Please close Notepad++ before continuing, then click OK.$\r$\n$\r$\n\
Click Cancel to abort installation." \
            IDOK checkagain IDCANCEL abortinstall
checkagain:
        FindWindow $0 "Notepad++"
        ${If} $0 != 0
            MessageBox MB_RETRYCANCEL|MB_ICONEXCLAMATION \
                "Notepad++ is still running. Please close it." \
                IDRETRY checkagain
        ${EndIf}
abortinstall:
        ${If} $0 != 0
            Abort
        ${EndIf}
    ${EndIf}
FunctionEnd

; ====================================================================
; Sections
; ====================================================================

Section "NppTreeSitter Plugin (required)" SecPlugin
    SectionIn RO

    SetOutPath "$INSTDIR\plugins\NppTreeSitter"
    File "${PACKAGE_DIR}\plugins\NppTreeSitter\NppTreeSitter.dll"

    SetOutPath "$APPDATA\Notepad++\plugins\config"
    File "${PACKAGE_DIR}\config\NppTreeSitter.xml"

    SetOutPath "$APPDATA\Notepad++\plugins\config\NppTreeSitter"
    File "${PACKAGE_DIR}\config\NppTreeSitter\styles.conf"

    WriteUninstaller "$INSTDIR\plugins\NppTreeSitter\uninstall-NppTreeSitter.exe"

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

; --- Grammar Sections (injected by generate_nsi.py) ---
SectionGroup "Grammars" SecGrammars

;GRAMMAR_SECTIONS_MARKER

SectionGroupEnd

; --- Section Descriptions ---
!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
    !insertmacro MUI_DESCRIPTION_TEXT ${SecPlugin} \
        "The core NppTreeSitter plugin DLL and configuration files. Required."
    !insertmacro MUI_DESCRIPTION_TEXT ${SecGrammars} \
        "Tree-sitter grammars for additional languages. Select the ones you need."
    ;GRAMMAR_DESCRIPTIONS_MARKER
!insertmacro MUI_FUNCTION_DESCRIPTION_END

; --- Uninstall ---
Section "Uninstall"
    Delete "$INSTDIR\NppTreeSitter.dll"
    Delete "$INSTDIR\uninstall-NppTreeSitter.exe"
    RMDir "$INSTDIR"

    RMDir /r "$APPDATA\Notepad++\plugins\config\NppTreeSitter"
    Delete "$APPDATA\Notepad++\plugins\config\NppTreeSitter.xml"

    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\NppTreeSitter"
SectionEnd
