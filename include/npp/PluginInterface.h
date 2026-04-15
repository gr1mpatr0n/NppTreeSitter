// include/npp/PluginInterface.h
// Minimal Notepad++ plugin API definitions.
// In a production build, replace with the full headers from
// https://github.com/notepad-plus-plus/notepad-plus-plus/tree/master/PowerEditor/src/MISC/PluginsManager
#pragma once

// This plugin always targets Windows (Notepad++ is Windows-only).
#include <windows.h>
#include <cstdint>

// ---- Notepad++ message constants ----

constexpr int NPPMSG                       = (WM_USER + 1000);
constexpr int NPPM_GETCURRENTSCINTILLA     = (NPPMSG + 4);
constexpr int NPPM_GETPLUGINSCONFIGDIR     = (NPPMSG + 46);
constexpr int NPPM_GETFILENAME             = (NPPMSG + 27);
constexpr int NPPM_GETFULLCURRENTPATH      = (NPPMSG + 16);
constexpr int NPPM_GETCURRENTLANGTYPE      = (NPPMSG + 5);

// Notepad++ notification codes
constexpr int NPPN_FIRST                   = 1000;
constexpr int NPPN_READY                   = (NPPN_FIRST + 1);
constexpr int NPPN_BUFFERACTIVATED         = (NPPN_FIRST + 9);
constexpr int NPPN_LANGCHANGED             = (NPPN_FIRST + 13);
constexpr int NPPN_FILEOPENED              = (NPPN_FIRST + 7);
constexpr int NPPN_SHUTDOWN                = (NPPN_FIRST + 11);

// ---- NppData: handles passed to setInfo ----
struct NppData {
    HWND nppHandle;
    HWND scintillaMainHandle;
    HWND scintillaSecondHandle;
};

// ---- Function item for the Plugins menu ----
typedef void (*PFUNCPLUGINCMD)();

struct ShortcutKey {
    bool isCtrl;
    bool isAlt;
    bool isShift;
    UCHAR key;
};

constexpr int nbChar = 64;

struct FuncItem {
    wchar_t        itemName[nbChar];
    PFUNCPLUGINCMD pFunc;
    int            cmdID;
    bool           init2Check;
    ShortcutKey*   pShKey;
};
