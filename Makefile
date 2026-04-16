# Makefile — convenience wrapper for cross-compiling NppTreeSitter + grammars
# from Arch Linux using MinGW-w64.
#
# Prerequisites (Arch):
#   sudo pacman -S mingw-w64-gcc cmake git bash
#
# Targets:
#   make plugin           — build the plugin DLL
#   make grammar-zig      — build the Zig grammar DLL
#   make grammar NAME=c   — build an arbitrary grammar (cloned from tree-sitter-grammars)

SHELL := /bin/bash
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
# v0.25.6 supports grammar ABI versions 13–15.
TS_VERSION  := v0.25.6

.PHONY: all plugin package clean install-wine

# Build plugin + any grammars in one invocation:
#   make plugin grammar-zig grammar-c grammar-cpp
#   make install-wine
all: plugin

# ============================================================================
# Plugin DLL
# ============================================================================
plugin:
	cmake -B $(PLUGIN_BUILD) \
	      -DCMAKE_TOOLCHAIN_FILE=$(TOOLCHAIN) \
	      -DCMAKE_BUILD_TYPE=Release
	cmake --build $(PLUGIN_BUILD) --config Release -j$$(nproc)
	@echo "✅ Plugin DLL: $(PLUGIN_BUILD)/NppTreeSitter.dll"

# Clone tree-sitter core (for headers + lib.c used by grammar compilation).
$(BUILD_DIR)/tree-sitter/lib/src/lib.c:
	git clone --depth 1 --branch $(TS_VERSION) \
	    https://github.com/tree-sitter/tree-sitter.git \
	    $(BUILD_DIR)/tree-sitter

# ============================================================================
# Grammar DLLs — pattern rule.
#
# Usage (single or multiple in one invocation):
#   make grammar-zig
#   make grammar-c grammar-cpp grammar-python
#   make plugin grammar-zig grammar-c install-wine
#
# Override the repository URL for a specific grammar:
#   make grammar-elixir REPO_elixir=https://github.com/elixir-lang/tree-sitter-elixir.git
#
# The Makefile includes a built-in URL map for grammars whose canonical
# repos are not under tree-sitter-grammars/ or tree-sitter/.  Override
# any entry by setting REPO_<lang> on the command line or in the CI env.
#
# Each grammar-<n> target:
#   1. Clones the grammar repo (using the URL map or fallback search)
#   2. Compiles parser.c (+ scanner.c if present) into grammar.dll
#
# The tree-sitter core is cloned once as a shared prerequisite.
# ============================================================================

# --- Grammar repository URL map ---
# Grammars not under tree-sitter-grammars/ or tree-sitter/ orgs.
REPO_elixir     ?= https://github.com/elixir-lang/tree-sitter-elixir.git
REPO_kotlin     ?= https://github.com/fwcd/tree-sitter-kotlin.git
REPO_dart       ?= https://github.com/UserNobwordy/tree-sitter-dart.git
REPO_gleam      ?= https://github.com/gleam-lang/tree-sitter-gleam.git
REPO_nix        ?= https://github.com/nix-community/tree-sitter-nix.git
REPO_vue        ?= https://github.com/tree-sitter-grammars/tree-sitter-vue.git
REPO_svelte     ?= https://github.com/tree-sitter-grammars/tree-sitter-svelte.git
REPO_odin       ?= https://github.com/tree-sitter-grammars/tree-sitter-odin.git
REPO_proto      ?= https://github.com/treywood/tree-sitter-proto.git
REPO_glsl       ?= https://github.com/tree-sitter-grammars/tree-sitter-glsl.git
REPO_wgsl       ?= https://github.com/szebniok/tree-sitter-wgsl.git
REPO_devicetree ?= https://github.com/joelspadin/tree-sitter-devicetree.git
REPO_meson      ?= https://github.com/tree-sitter-grammars/tree-sitter-meson.git
REPO_just       ?= https://github.com/IndianBoy42/tree-sitter-just.git
REPO_kdl        ?= https://github.com/tree-sitter-grammars/tree-sitter-kdl.git
REPO_astro      ?= https://github.com/virchau13/tree-sitter-astro.git
REPO_dockerfile ?= https://github.com/camdencheek/tree-sitter-dockerfile.git
REPO_hcl        ?= https://github.com/tree-sitter-grammars/tree-sitter-hcl.git
REPO_ocaml      ?= https://github.com/tree-sitter/tree-sitter-ocaml.git

grammar-%: $(BUILD_DIR)/tree-sitter/lib/src/lib.c
	@set -e; \
	lang="$*"; \
	mkdir -p "$(GRAMMAR_DIR)/out/$$lang"; \
	if [ ! -d "$(GRAMMAR_DIR)/tree-sitter-$$lang" ]; then \
	    echo "Cloning tree-sitter-$$lang..."; \
	    repo_var="REPO_$$lang"; \
	    repo="$(REPO_$*)"; \
	    if [ -n "$$repo" ]; then \
	        GIT_TERMINAL_PROMPT=0 git clone --depth 1 "$$repo" \
	            "$(GRAMMAR_DIR)/tree-sitter-$$lang" \
	        || { echo "❌ $$lang: failed to clone $$repo"; exit 1; }; \
	    elif GIT_TERMINAL_PROMPT=0 git clone --depth 1 \
	        "https://github.com/tree-sitter-grammars/tree-sitter-$$lang.git" \
	        "$(GRAMMAR_DIR)/tree-sitter-$$lang" 2>/dev/null; then \
	        true; \
	    elif GIT_TERMINAL_PROMPT=0 git clone --depth 1 \
	        "https://github.com/tree-sitter/tree-sitter-$$lang.git" \
	        "$(GRAMMAR_DIR)/tree-sitter-$$lang" 2>/dev/null; then \
	        true; \
	    else \
	        echo "❌ $$lang: failed to clone grammar repository"; \
	        echo "   Set REPO_$$lang=<url> to specify the correct repository."; \
	        exit 1; \
	    fi; \
	fi; \
	if [ ! -f "$(GRAMMAR_DIR)/tree-sitter-$$lang/src/parser.c" ]; then \
	    echo "❌ $$lang: parser.c not found in cloned repository"; \
	    exit 1; \
	fi; \
	SCANNER=""; \
	if [ -f "$(GRAMMAR_DIR)/tree-sitter-$$lang/src/scanner.c" ]; then \
	    SCANNER="$(GRAMMAR_DIR)/tree-sitter-$$lang/src/scanner.c"; \
	fi; \
	$(MINGW_CC) -shared -O2 -DNDEBUG \
	    -I $(BUILD_DIR)/tree-sitter/lib/include \
	    -I $(BUILD_DIR)/tree-sitter/lib/src \
	    "$(GRAMMAR_DIR)/tree-sitter-$$lang/src/parser.c" \
	    $$SCANNER \
	    $(BUILD_DIR)/tree-sitter/lib/src/lib.c \
	    -o "$(GRAMMAR_DIR)/out/$$lang/grammar.dll" \
	    -Wl,--export-all-symbols \
	    -static-libgcc && \
	echo "✅ $$lang grammar DLL: $(GRAMMAR_DIR)/out/$$lang/grammar.dll"



# ============================================================================
# Package — assemble the deployment tree.
#
# Discovers ALL grammars that have been built under build/grammars/out/
# and packages them, along with their highlights.scm and extensions.txt.
# Also generates the NppTreeSitter.xml with a <LexerType> per grammar.
# ============================================================================
package: plugin
	@echo "Assembling package..."
	@mkdir -p $(PACKAGE_DIR)/plugins/NppTreeSitter
	@mkdir -p $(PACKAGE_DIR)/config/NppTreeSitter

	@# Plugin DLL
	cp $(PLUGIN_BUILD)/NppTreeSitter.dll \
	   $(PACKAGE_DIR)/plugins/NppTreeSitter/

	@# Global style map
	cp config/styles.conf $(PACKAGE_DIR)/config/NppTreeSitter/

	@# --- Discover and package all built grammars ---
	@found=0; \
	for dll in $(GRAMMAR_DIR)/out/*/grammar.dll; do \
	    [ -f "$$dll" ] || continue; \
	    lang=$$(basename $$(dirname "$$dll")); \
	    echo "  Packaging grammar: $$lang"; \
	    mkdir -p "$(PACKAGE_DIR)/config/NppTreeSitter/grammars/$$lang"; \
	    cp "$$dll" "$(PACKAGE_DIR)/config/NppTreeSitter/grammars/$$lang/"; \
	    src="$(GRAMMAR_DIR)/tree-sitter-$$lang"; \
	    if [ -f "$$src/queries/highlights.scm" ]; then \
	        cp "$$src/queries/highlights.scm" \
	           "$(PACKAGE_DIR)/config/NppTreeSitter/grammars/$$lang/"; \
	    fi; \
	    if [ -f "$$src/queries/locals.scm" ]; then \
	        cp "$$src/queries/locals.scm" \
	           "$(PACKAGE_DIR)/config/NppTreeSitter/grammars/$$lang/"; \
	    fi; \
	    if [ -f "$$src/tree-sitter.json" ]; then \
	        base_hl_paths=$$(grep -oP '"node_modules/tree-sitter-\K[^/]+(?=/queries/highlights\.scm")' \
	                         "$$src/tree-sitter.json" 2>/dev/null); \
	        if [ -n "$$base_hl_paths" ]; then \
	            > "$(PACKAGE_DIR)/config/NppTreeSitter/grammars/$$lang/base_highlights.scm"; \
	            for base_lang in $$base_hl_paths; do \
	                echo "    ↳ inherits highlights from: $$base_lang"; \
	                base_src="$(GRAMMAR_DIR)/tree-sitter-$$base_lang"; \
	                if [ ! -d "$$base_src" ]; then \
	                    echo "    ↳ cloning tree-sitter-$$base_lang for base queries..."; \
	                    git clone --depth 1 \
	                        "https://github.com/tree-sitter-grammars/tree-sitter-$$base_lang.git" \
	                        "$$base_src" 2>/dev/null \
	                    || git clone --depth 1 \
	                        "https://github.com/tree-sitter/tree-sitter-$$base_lang.git" \
	                        "$$base_src" 2>/dev/null; \
	                fi; \
	                if [ -f "$$base_src/queries/highlights.scm" ]; then \
	                    cat "$$base_src/queries/highlights.scm" \
	                        >> "$(PACKAGE_DIR)/config/NppTreeSitter/grammars/$$lang/base_highlights.scm"; \
	                    echo "" >> "$(PACKAGE_DIR)/config/NppTreeSitter/grammars/$$lang/base_highlights.scm"; \
	                fi; \
	            done; \
	        fi; \
	    fi; \
	    if [ ! -f "$(PACKAGE_DIR)/config/NppTreeSitter/grammars/$$lang/extensions.txt" ]; then \
	        if   [ "$$lang" = "zig" ];        then printf '.zig\n.zon\n'; \
	        elif [ "$$lang" = "c" ];          then printf '.c\n.h\n'; \
	        elif [ "$$lang" = "cpp" ];        then printf '.cpp\n.hpp\n.cc\n.hh\n'; \
	        elif [ "$$lang" = "rust" ];       then printf '.rs\n'; \
	        elif [ "$$lang" = "python" ];     then printf '.py\n.pyw\n'; \
	        elif [ "$$lang" = "go" ];         then printf '.go\n'; \
	        elif [ "$$lang" = "javascript" ]; then printf '.js\n.mjs\n.cjs\n'; \
	        elif [ "$$lang" = "typescript" ]; then printf '.ts\n.mts\n.cts\n'; \
	        elif [ "$$lang" = "lua" ];        then printf '.lua\n'; \
	        elif [ "$$lang" = "bash" ];       then printf '.sh\n.bash\n'; \
	        elif [ "$$lang" = "json" ];       then printf '.json\n'; \
	        elif [ "$$lang" = "toml" ];       then printf '.toml\n'; \
	        elif [ "$$lang" = "yaml" ];       then printf '.yml\n.yaml\n'; \
	        else printf '.%s\n' "$$lang"; \
	        fi > "$(PACKAGE_DIR)/config/NppTreeSitter/grammars/$$lang/extensions.txt"; \
	    fi; \
	    found=$$((found + 1)); \
	done; \
	if [ "$$found" -eq 0 ]; then \
	    echo "  ⚠  No grammars built yet. Run 'make grammar-zig' or 'make grammar NAME=c' first."; \
	fi

	@# --- Generate NppTreeSitter.xml with a <LexerType> per grammar ---
	@# Notepad++ REQUIRES this file or it refuses to load the lexer plugin.
	@# The plugin also auto-generates it at runtime, but shipping a static
	@# copy avoids the chicken-and-egg problem on first install.
	@echo '<?xml version="1.0" encoding="UTF-8" ?>' \
	    > $(PACKAGE_DIR)/config/NppTreeSitter.xml
	@echo '<!-- Auto-generated by NppTreeSitter build. -->' \
	    >> $(PACKAGE_DIR)/config/NppTreeSitter.xml
	@echo '<NotepadPlus>' >> $(PACKAGE_DIR)/config/NppTreeSitter.xml
	@echo '    <LexerStyles>' >> $(PACKAGE_DIR)/config/NppTreeSitter.xml
	@for extfile in $(PACKAGE_DIR)/config/NppTreeSitter/grammars/*/extensions.txt; do \
	    [ -f "$$extfile" ] || continue; \
	    lang=$$(basename $$(dirname "$$extfile")); \
	    exts=""; \
	    while IFS= read -r ext || [ -n "$$ext" ]; do \
	        ext=$${ext#.}; \
	        ext=$$(echo "$$ext" | tr -d '\r '); \
	        [ -z "$$ext" ] && continue; \
	        [ -n "$$exts" ] && exts="$$exts "; \
	        exts="$$exts$$ext"; \
	    done < "$$extfile"; \
	    echo "        <LexerType name=\"$$lang\" desc=\"$$lang (TreeSitter)\" ext=\"$$exts\">" \
	        >> $(PACKAGE_DIR)/config/NppTreeSitter.xml; \
	    for id_name_fg in \
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
	    done; \
	    echo '        </LexerType>' >> $(PACKAGE_DIR)/config/NppTreeSitter.xml; \
	done
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
	@# Remove old config so new grammars + XML are picked up cleanly.
	rm -rf "$(WINE_APPDATA)/plugins/config/NppTreeSitter"
	rm -f  "$(WINE_APPDATA)/plugins/config/NppTreeSitter.xml"
	cp -r $(PACKAGE_DIR)/config/NppTreeSitter \
	   "$(WINE_APPDATA)/plugins/config/"
	cp $(PACKAGE_DIR)/config/NppTreeSitter.xml \
	   "$(WINE_APPDATA)/plugins/config/"
	@echo ""
	@echo "✅ Installed.  Grammars:"
	@ls -1 "$(WINE_APPDATA)/plugins/config/NppTreeSitter/grammars/" 2>/dev/null \
	    | sed 's/^/   /' || echo "   (none)"
	@echo ""
	@echo "Launch Notepad++ with:"
	@echo "   wine \"$(WINE_NPP)/notepad++.exe\""
