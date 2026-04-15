# Makefile — convenience wrapper for cross-compiling NppTreeSitter + grammars
# from Arch Linux using MinGW-w64.
#
# Prerequisites (Arch):
#   sudo pacman -S mingw-w64-gcc cmake git
#
# Targets:
#   make plugin           — build the plugin DLL
#   make grammar-zig      — build the Zig grammar DLL
#   make grammar NAME=c   — build an arbitrary grammar (cloned from tree-sitter-grammars)
#   make all              — plugin + zig grammar
#   make package          — assemble a ready-to-deploy directory tree
#   make clean            — remove build artifacts
#
# The output lands in build/package/ with the correct directory layout.

TOOLCHAIN   := cmake/mingw-w64-x86_64.cmake
BUILD_DIR   := build
PLUGIN_BUILD:= $(BUILD_DIR)/plugin
GRAMMAR_DIR := $(BUILD_DIR)/grammars
PACKAGE_DIR := $(BUILD_DIR)/package
MINGW_CC    := x86_64-w64-mingw32-gcc
MINGW_CXX   := x86_64-w64-mingw32-g++

# tree-sitter version to fetch (must match the plugin's FetchContent).
TS_VERSION  := v0.24.7

.PHONY: all plugin grammar-zig grammar package clean

all: plugin grammar-zig package

# ============================================================================
# Plugin DLL
# ============================================================================
plugin:
	cmake -B $(PLUGIN_BUILD) \
	      -DCMAKE_TOOLCHAIN_FILE=$(TOOLCHAIN) \
	      -DCMAKE_BUILD_TYPE=Release
	cmake --build $(PLUGIN_BUILD) --config Release -j$$(nproc)
	@echo "✅ Plugin DLL: $(PLUGIN_BUILD)/NppTreeSitter.dll"

# ============================================================================
# Grammar DLL — Zig
# ============================================================================
grammar-zig: $(GRAMMAR_DIR)/tree-sitter-zig/src/parser.c $(BUILD_DIR)/tree-sitter/lib/src/lib.c
	@mkdir -p $(GRAMMAR_DIR)/out/zig
	$(MINGW_CC) -shared -O2 -DNDEBUG \
	    -I $(BUILD_DIR)/tree-sitter/lib/include \
	    -I $(BUILD_DIR)/tree-sitter/lib/src \
	    $(GRAMMAR_DIR)/tree-sitter-zig/src/parser.c \
	    $(BUILD_DIR)/tree-sitter/lib/src/lib.c \
	    -o $(GRAMMAR_DIR)/out/zig/grammar.dll \
	    -Wl,--export-all-symbols \
	    -static-libgcc
	@echo "✅ Zig grammar DLL: $(GRAMMAR_DIR)/out/zig/grammar.dll"

# Clone the Zig grammar if not present.
$(GRAMMAR_DIR)/tree-sitter-zig/src/parser.c:
	@mkdir -p $(GRAMMAR_DIR)
	git clone --depth 1 --branch v1.1.2 \
	    https://github.com/tree-sitter-grammars/tree-sitter-zig.git \
	    $(GRAMMAR_DIR)/tree-sitter-zig

# Clone tree-sitter core (for headers + lib.c used by grammar compilation).
$(BUILD_DIR)/tree-sitter/lib/src/lib.c:
	git clone --depth 1 --branch $(TS_VERSION) \
	    https://github.com/tree-sitter/tree-sitter.git \
	    $(BUILD_DIR)/tree-sitter

# ============================================================================
# Generic grammar target.
#
# Usage:  make grammar NAME=python
#         make grammar NAME=c
#
# Expects the grammar repo to follow the standard naming convention:
#   https://github.com/tree-sitter-grammars/tree-sitter-<NAME>
#   or https://github.com/tree-sitter/tree-sitter-<NAME>
#
# If the grammar has src/scanner.c, it is automatically included.
# ============================================================================
grammar: $(BUILD_DIR)/tree-sitter/lib/src/lib.c
ifndef NAME
	$(error Set NAME=<language>, e.g. make grammar NAME=python)
endif
	@mkdir -p $(GRAMMAR_DIR)/out/$(NAME)
	@if [ ! -d "$(GRAMMAR_DIR)/tree-sitter-$(NAME)" ]; then \
	    echo "Cloning tree-sitter-$(NAME)..."; \
	    git clone --depth 1 \
	        "https://github.com/tree-sitter-grammars/tree-sitter-$(NAME).git" \
	        "$(GRAMMAR_DIR)/tree-sitter-$(NAME)" 2>/dev/null \
	    || git clone --depth 1 \
	        "https://github.com/tree-sitter/tree-sitter-$(NAME).git" \
	        "$(GRAMMAR_DIR)/tree-sitter-$(NAME)"; \
	fi
	@# Build list of source files — always parser.c, optionally scanner.c
	@SCANNER=""; \
	if [ -f "$(GRAMMAR_DIR)/tree-sitter-$(NAME)/src/scanner.c" ]; then \
	    SCANNER="$(GRAMMAR_DIR)/tree-sitter-$(NAME)/src/scanner.c"; \
	fi; \
	$(MINGW_CC) -shared -O2 -DNDEBUG \
	    -I $(BUILD_DIR)/tree-sitter/lib/include \
	    -I $(BUILD_DIR)/tree-sitter/lib/src \
	    $(GRAMMAR_DIR)/tree-sitter-$(NAME)/src/parser.c \
	    $$SCANNER \
	    $(BUILD_DIR)/tree-sitter/lib/src/lib.c \
	    -o $(GRAMMAR_DIR)/out/$(NAME)/grammar.dll \
	    -Wl,--export-all-symbols \
	    -static-libgcc
	@echo "✅ $(NAME) grammar DLL: $(GRAMMAR_DIR)/out/$(NAME)/grammar.dll"

# ============================================================================
# Package — assemble the deployment tree
# ============================================================================
package: plugin grammar-zig
	@echo "Assembling package..."
	@mkdir -p $(PACKAGE_DIR)/plugins/NppTreeSitter
	@mkdir -p $(PACKAGE_DIR)/config/NppTreeSitter/grammars/zig

	@# Plugin DLL
	cp $(PLUGIN_BUILD)/NppTreeSitter.dll \
	   $(PACKAGE_DIR)/plugins/NppTreeSitter/

	@# Zig grammar
	cp $(GRAMMAR_DIR)/out/zig/grammar.dll \
	   $(PACKAGE_DIR)/config/NppTreeSitter/grammars/zig/
	cp $(GRAMMAR_DIR)/tree-sitter-zig/queries/highlights.scm \
	   $(PACKAGE_DIR)/config/NppTreeSitter/grammars/zig/
	@# Copy locals.scm if it exists
	@if [ -f "$(GRAMMAR_DIR)/tree-sitter-zig/queries/locals.scm" ]; then \
	    cp $(GRAMMAR_DIR)/tree-sitter-zig/queries/locals.scm \
	       $(PACKAGE_DIR)/config/NppTreeSitter/grammars/zig/; \
	fi
	cp config/grammars/rust/extensions.txt \
	   $(PACKAGE_DIR)/config/NppTreeSitter/grammars/zig/extensions.txt 2>/dev/null \
	   || printf '.zig\n.zon\n' > $(PACKAGE_DIR)/config/NppTreeSitter/grammars/zig/extensions.txt

	@# Global style map
	cp config/styles.conf $(PACKAGE_DIR)/config/NppTreeSitter/

	@# Generate NppTreeSitter.xml — Notepad++ REQUIRES this file in
	@# plugins\Config\ or it refuses to load the lexer plugin.
	@# The plugin auto-generates it at runtime too, but shipping a
	@# static copy avoids the chicken-and-egg problem on first install.
	@echo '<?xml version="1.0" encoding="UTF-8" ?>' \
	    > $(PACKAGE_DIR)/config/NppTreeSitter.xml
	@echo '<!-- Auto-generated by NppTreeSitter build. -->' \
	    >> $(PACKAGE_DIR)/config/NppTreeSitter.xml
	@echo '<NotepadPlus>' >> $(PACKAGE_DIR)/config/NppTreeSitter.xml
	@echo '    <LexerStyles>' >> $(PACKAGE_DIR)/config/NppTreeSitter.xml
	@echo '        <LexerType name="zig" desc="Zig (TreeSitter)" ext="zig zon">' \
	    >> $(PACKAGE_DIR)/config/NppTreeSitter.xml
	@for id_name_fg in \
	    "0:Default:000000:0" "1:Keyword:0000FF:2" "2:Keyword (function):0000FF:2" \
	    "3:Keyword (return):0000FF:2" "4:Keyword (operator):8B008B:0" \
	    "5:String:A31515:0" "6:String (special):B56508:0" "7:Number:098658:0" \
	    "8:Float:098658:0" "9:Function:795E26:0" "10:Function (builtin):795E26:1" \
	    "11:Function (call):795E26:0" "12:Method:795E26:0" "13:Method (call):795E26:0" \
	    "14:Type:267F99:0" "15:Type (builtin):267F99:1" "16:Type (qualifier):267F99:2" \
	    "17:Variable:001080:0" "18:Variable (builtin):001080:1" \
	    "19:Variable (parameter):001080:1" "20:Property:001080:0" "21:Field:001080:0" \
	    "22:Constant:0070C1:2" "23:Constant (builtin):0070C1:3" "24:Comment:008000:0" \
	    "25:Operator:000000:0" "26:Punctuation:505050:0" "27:Bracket:AF00DB:0" \
	    "28:Delimiter:505050:0" "29:Label:8B008B:0" "30:Attribute:267F99:1" \
	    "31:Constructor:267F99:0"; \
	do \
	    IFS=':' read -r sid sname sfg sfs <<< "$$id_name_fg"; \
	    echo "            <WordsStyle styleID=\"$$sid\" name=\"$$sname\" fgColor=\"$$sfg\" fontStyle=\"$$sfs\" />" \
	        >> $(PACKAGE_DIR)/config/NppTreeSitter.xml; \
	done
	@echo '        </LexerType>' >> $(PACKAGE_DIR)/config/NppTreeSitter.xml
	@echo '    </LexerStyles>' >> $(PACKAGE_DIR)/config/NppTreeSitter.xml
	@echo '</NotepadPlus>' >> $(PACKAGE_DIR)/config/NppTreeSitter.xml

	@echo ""
	@echo "✅ Package ready in $(PACKAGE_DIR)/"
	@echo ""
	@echo "Deploy on Windows:"
	@echo "  Copy plugins/NppTreeSitter/         → <Notepad++ install>/plugins/"
	@echo "  Copy config/NppTreeSitter/          → %APPDATA%\\Notepad++\\plugins\\config\\"
	@echo "  Copy config/NppTreeSitter.xml       → %APPDATA%\\Notepad++\\plugins\\config\\"
	@echo ""
	@echo "Under Wine:"
	@echo "  WINEPREFIX=~/.wine  (or your custom prefix)"
	@echo "  Copy plugins/NppTreeSitter/         → \$$WINEPREFIX/drive_c/Program Files/Notepad++/plugins/"
	@echo "  Copy config/NppTreeSitter/          → \$$WINEPREFIX/drive_c/users/\$$(whoami)/AppData/Roaming/Notepad++/plugins/config/"
	@echo "  Copy config/NppTreeSitter.xml       → \$$WINEPREFIX/drive_c/users/\$$(whoami)/AppData/Roaming/Notepad++/plugins/config/"

# ============================================================================
# Clean
# ============================================================================
clean:
	rm -rf $(BUILD_DIR)

# ============================================================================
# Install into a Wine prefix (convenience target for testing)
# ============================================================================
WINEPREFIX     ?= $(HOME)/.wine
WINE_USER      := $(shell whoami)
WINE_NPP       := $(WINEPREFIX)/drive_c/Program Files/Notepad++
WINE_APPDATA   := $(WINEPREFIX)/drive_c/users/$(WINE_USER)/AppData/Roaming/Notepad++

install-wine: package
	@echo "Installing into Wine prefix: $(WINEPREFIX)"
	mkdir -p "$(WINE_NPP)/plugins/NppTreeSitter"
	cp $(PACKAGE_DIR)/plugins/NppTreeSitter/NppTreeSitter.dll \
	   "$(WINE_NPP)/plugins/NppTreeSitter/"
	mkdir -p "$(WINE_APPDATA)/plugins/config"
	cp -r $(PACKAGE_DIR)/config/NppTreeSitter \
	   "$(WINE_APPDATA)/plugins/config/"
	cp $(PACKAGE_DIR)/config/NppTreeSitter.xml \
	   "$(WINE_APPDATA)/plugins/config/"
	@echo ""
	@echo "✅ Installed.  Launch Notepad++ with:"
	@echo "   wine \"$(WINE_NPP)/notepad++.exe\""
