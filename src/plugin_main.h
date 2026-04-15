// src/plugin_main.h
#pragma once

#include "npp/PluginInterface.h"
#include "scintilla/Scintilla.h"
#include "grammar_registry.h"
#include "style_map.h"

namespace npp_ts {

// Notepad++ plugin interface.
void             plugin_setInfo(NppData nppData);
const wchar_t*   plugin_getName();
FuncItem*        plugin_getFuncsArray(int* nbF);
void             plugin_beNotified(SCNotification* notification);
LRESULT          plugin_messageProc(UINT msg, WPARAM wParam, LPARAM lParam);
BOOL             plugin_isUnicode();

// Lexilla entry points.
const GrammarRegistry& plugin_get_registry();
const StyleMap&        plugin_get_style_map();

} // namespace npp_ts
