// include/scintilla/Scintilla.h
// Minimal subset of Scintilla constants needed by the plugin.
// Types (Sci_Position etc.) are defined in Sci_Position.h (included via ILexer.h).
#pragma once

#include <cstdint>
#include <windows.h>

#include "Sci_Position.h"

// --- A handful of the SCI_xxx message constants we actually use. ---
constexpr int SCI_GETLENGTH              = 2006;
constexpr int SCI_GETCHARACTERPOINTER    = 2520;
constexpr int SCI_STARTSTYLING           = 2032;
constexpr int SCI_SETSTYLING             = 2033;
constexpr int SCI_SETSTYLINGEX           = 2073;
constexpr int SCI_SETILEXER              = 4033;
constexpr int SCI_GETLINECOUNT           = 2154;
constexpr int SCI_LINEFROMPOSITION       = 2166;
constexpr int SCI_POSITIONFROMLINE       = 2167;
constexpr int SCI_GETLINEENDPOSITION     = 2136;
constexpr int SCI_SETFOLDLEVEL           = 2222;
constexpr int SCI_GETFOLDLEVEL           = 2223;
constexpr int SCI_SETPROPERTY            = 4004;
constexpr int SCI_GETPROPERTY            = 4008;
constexpr int SCI_COLOURISE              = 4003;

// Fold level flags
constexpr int SC_FOLDLEVELBASE          = 0x400;
constexpr int SC_FOLDLEVELHEADERFLAG    = 0x2000;
constexpr int SC_FOLDLEVELNUMBERMASK    = 0x0FFF;

// Style constants
constexpr int STYLE_DEFAULT              = 32;
constexpr int STYLE_MAX                  = 255;

// --- SCN notification codes we hook ---
constexpr int SCN_MODIFIED               = 2008;

// Modification flags
constexpr int SC_MOD_INSERTTEXT          = 0x1;
constexpr int SC_MOD_DELETETEXT          = 0x2;

// SCNotification structure (simplified — only fields we actually access)
struct SCNotification {
    void*        nmhdr_hwndFrom;
    uintptr_t    nmhdr_idFrom;
    unsigned int nmhdr_code;

    Sci_Position position;
    int          modificationType;
    const char*  text;
    Sci_Position length;
    Sci_Position linesAdded;
};
