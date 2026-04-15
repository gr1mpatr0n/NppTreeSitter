// src/plugin_exports.cpp
// C-linkage DLL exports that Notepad++ calls.
//
// Two sets of exports co-exist in this DLL:
//
// 1. Standard Notepad++ plugin exports:
//      setInfo, getName, getFuncsArray, beNotified, messageProc, isUnicode
//
// 2. Lexilla-style lexer exports:
//      GetLexerCount, GetLexerName, GetLexerStatusText, CreateLexer
//
// Notepad++ detects the presence of GetLexerCount to decide that we are a
// "lexer plugin" and loads our grammar names into the Language menu.

#include "plugin_main.h"
#include "ts_lexer_bridge.h"

#include <cstring>
#include <cwchar>
#include <string>
#include <vector>
#include <algorithm>

#include <windows.h>

// ---------------------------------------------------------------------------
// Portable DLL export macro.
// Both MSVC and MinGW support __declspec(dllexport).
// ---------------------------------------------------------------------------
#define NPP_EXPORT extern "C" __declspec(dllexport)

// On x64 there is only one calling convention, so __stdcall / WINAPI is
// silently ignored.  On x86, the Lexilla exports use __stdcall.
// WINAPI resolves to __stdcall on both MSVC and MinGW.

// ---------------------------------------------------------------------------
// Portable string helpers.
// strncpy_s / wcsncpy_s / wcscpy_s are MSVC-specific extensions to the
// C runtime.  We use standard C equivalents that work on MinGW.
// ---------------------------------------------------------------------------
static void safe_strncpy(char* dst, int dst_size, const char* src) {
    if (dst_size <= 0) return;
    std::strncpy(dst, src, static_cast<size_t>(dst_size - 1));
    dst[dst_size - 1] = '\0';
}

static void safe_wcsncpy(wchar_t* dst, int dst_size, const wchar_t* src) {
    if (dst_size <= 0) return;
    std::wcsncpy(dst, src, static_cast<size_t>(dst_size - 1));
    dst[dst_size - 1] = L'\0';
}

// ============================================================================
// DllMain
// ============================================================================

extern "C" BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
    switch (reason) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        break;
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

// ============================================================================
// Standard Notepad++ plugin exports
// ============================================================================

NPP_EXPORT void setInfo(NppData nppData) {
    npp_ts::plugin_setInfo(nppData);
}

NPP_EXPORT const wchar_t* getName() {
    return npp_ts::plugin_getName();
}

NPP_EXPORT FuncItem* getFuncsArray(int* nbF) {
    return npp_ts::plugin_getFuncsArray(nbF);
}

NPP_EXPORT void beNotified(SCNotification* notification) {
    npp_ts::plugin_beNotified(notification);
}

NPP_EXPORT LRESULT messageProc(UINT msg, WPARAM wParam, LPARAM lParam) {
    return npp_ts::plugin_messageProc(msg, wParam, lParam);
}

NPP_EXPORT BOOL isUnicode() {
    return npp_ts::plugin_isUnicode();
}

// ============================================================================
// Lexilla-style lexer exports
// ============================================================================

// Helper: get the list of registered grammar names (cached after first call).
static const std::vector<std::string>& grammar_names() {
    static std::vector<std::string> names;
    static bool cached = false;
    if (!cached) {
        names = npp_ts::plugin_get_registry().names();
        cached = true;
    }
    return names;
}

NPP_EXPORT int WINAPI GetLexerCount() {
    return static_cast<int>(grammar_names().size());
}

NPP_EXPORT void WINAPI GetLexerName(unsigned int index, char* name, int buflength) {
    auto& names = grammar_names();
    if (index < names.size()) {
        safe_strncpy(name, buflength, names[index].c_str());
    } else if (buflength > 0) {
        name[0] = '\0';
    }
}

NPP_EXPORT void WINAPI GetLexerStatusText(unsigned int index, wchar_t* desc, int buflength) {
    auto& names = grammar_names();
    if (index < names.size()) {
        std::wstring wide(names[index].begin(), names[index].end());
        wide = L"TreeSitter: " + wide;
        safe_wcsncpy(desc, buflength, wide.c_str());
    } else if (buflength > 0) {
        desc[0] = L'\0';
    }
}

NPP_EXPORT Scintilla::ILexer5* WINAPI CreateLexer(const char* name) {
    auto& reg = npp_ts::plugin_get_registry();
    auto* info = reg.find_by_name(name ? name : "");
    if (!info) return nullptr;

    auto& styles = npp_ts::plugin_get_style_map();
    return new npp_ts::TreeSitterLexer(info, &styles);
}
