# NppTreeSitter

A Notepad++ plugin that brings [tree-sitter](https://tree-sitter.github.io/tree-sitter/)
based syntax highlighting to Notepad++.

## How it works

NppTreeSitter implements the **ILexer5** interface (Scintilla 5 / Lexilla) and
registers itself as a lexer plugin.  When Notepad++ asks the plugin to style
a document, the plugin:

1. Parses the document text with tree-sitter.  A fresh full parse is
   performed on every `Lex()` call — no stale tree state is cached between
   edits, so highlighting is always correct regardless of edit pattern.
2. Runs the grammar's `highlights.scm` query (and optional `base_highlights.scm`
   for grammars that inherit from another, like C++ inheriting from C) to
   identify capture names (e.g. `@keyword`, `@function`, `@string`).
3. Maps each capture name to one of 32 Scintilla style slots via `styles.conf`.
4. Applies the styles through the `IDocument` interface.
5. Derives fold regions from the syntax tree (any named node spanning multiple
   lines becomes a fold header).

The grammars themselves — the `.dll` parser libraries and `.scm` query files —
are loaded at runtime from a config directory, so you can add support for any
language that has a tree-sitter grammar without recompiling the plugin.

A `NppTreeSitter.xml` style definition file is auto-generated on first run
(or at build time) so Notepad++ can register the languages in its Language
menu and Style Configurator.

## Building (Arch Linux / MinGW cross-compilation)

### Prerequisites

```bash
sudo pacman -S mingw-w64-gcc cmake git bash
```

### Quick start

```bash
make plugin                # build the plugin DLL
make grammar NAME=zig      # build the Zig grammar
make grammar NAME=c        # build the C grammar
make grammar NAME=cpp      # build C++ (auto-detects C base query dependency)
make install-wine          # package + install into Wine prefix
```

The `install-wine` target assembles a deployment package, copies it into your
Wine prefix, and prints the installed grammars.

### What `make grammar` does

1. Clones the grammar repo from `tree-sitter-grammars` (or `tree-sitter`) on
   GitHub.
2. Cross-compiles `parser.c` (and `scanner.c` if present) into `grammar.dll`
   using MinGW, linking tree-sitter v0.25.6 statically.
3. For grammars that inherit queries from a base grammar (detected via
   `tree-sitter.json`), the base grammar is automatically cloned and its
   `highlights.scm` is saved as `base_highlights.scm`.

### MSVC builds (Windows native)

```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

For a 32-bit build, change `-A x64` to `-A Win32`.

### Using a local tree-sitter checkout

```bash
cmake -B build -DTREESITTER_ROOT="/path/to/tree-sitter" ...
```

## Installation

### From a build

```bash
make plugin
make grammar NAME=zig
make grammar NAME=c
make grammar NAME=cpp
make install-wine
```

### Manual installation

1. Copy `NppTreeSitter.dll` into `<Notepad++>/plugins/NppTreeSitter/`.

2. Create the config directory and add grammars:

```
%APPDATA%\Notepad++\plugins\config\
├── NppTreeSitter.xml              # auto-generated style definitions
└── NppTreeSitter\
    ├── styles.conf                # capture → style mapping
    └── grammars\
        ├── zig\
        │   ├── grammar.dll        # compiled parser (exports tree_sitter_zig)
        │   ├── highlights.scm     # highlight queries
        │   └── extensions.txt     # .zig / .zon
        ├── cpp\
        │   ├── grammar.dll        # compiled parser (exports tree_sitter_cpp)
        │   ├── highlights.scm     # C++-specific highlight queries
        │   ├── base_highlights.scm # C highlight queries (inherited)
        │   └── extensions.txt     # .cpp / .hpp / .cc / .hh
        └── ...
```

3. Restart Notepad++.  Each grammar appears in the **Language** menu.

### Under Wine

The default Wine prefix places the config at:

```
~/.wine/drive_c/users/<you>/AppData/Roaming/Notepad++/plugins/config/
```

`make install-wine` handles this automatically.  Set `WINEPREFIX` if you
use a non-default prefix:

```bash
WINEPREFIX=~/.wine-npp make install-wine
```

## Query inheritance (base_highlights.scm)

Some tree-sitter grammars layer their highlight queries on top of a base
grammar.  For example, tree-sitter-cpp's `tree-sitter.json` declares:

```json
"highlights": [
    "node_modules/tree-sitter-c/queries/highlights.scm",
    "queries/highlights.scm"
]
```

The Makefile detects this automatically during packaging: it clones the base
grammar, extracts its `highlights.scm`, and saves it as `base_highlights.scm`
alongside the derived grammar's own `highlights.scm`.

At runtime, the plugin prepends `base_highlights.scm` to `highlights.scm`
before compiling the query, matching tree-sitter's concatenation convention.

This also applies to grammars like TypeScript (inherits from JavaScript).

## Configuration

### styles.conf

Maps tree-sitter capture names to Scintilla style IDs (0–31):

```ini
# capture_name = style_id
keyword = 1
function = 9
comment = 24
```

Dotted names are resolved with prefix fallback: if `function.builtin` isn't
in the map, `function` is tried.

### NppTreeSitter.xml

Notepad++ requires a style XML file named `NppTreeSitter.xml` in
`plugins\Config\`.  The plugin auto-generates this on first run from the
installed grammars.  The Makefile also generates it at build time.

To regenerate after adding grammars, delete the existing XML and restart
Notepad++ (or re-run `make install-wine`).

Colours can be customised via **Settings → Style Configurator** in Notepad++.

## Architecture

```
plugin_exports.cpp    ← C-linkage DLL entry points (Npp + Lexilla)
plugin_main.cpp       ← Plugin lifecycle, singletons, style XML generation
ts_lexer_bridge.cpp   ← ILexer5 implementation (Lex / Fold / incremental parse)
ts_query_runner.cpp   ← Runs highlights.scm queries, produces styled ranges
style_map.cpp         ← Capture name → style ID mapping
grammar_registry.cpp  ← Discovers & loads grammar DLLs + .scm files
```

### Data flow

```
Notepad++ ──► CreateLexer("zig")
                │
                ▼
          TreeSitterLexer(grammar_info, style_map)
                │
  Scintilla ──► Lex(startPos, length, …, IDocument*)
                │
                ├─► ts_parser_parse_string(…)      [full reparse]
                ├─► run_highlight_query(…)          [query captures]
                ├─► IDocument::StartStyling()       [apply styles]
                │   IDocument::SetStyleFor()
                └─► ts_tree_delete(…)               [no cached state]

  Scintilla ──► Fold(…)
                │
                ├─► ts_parser_parse_string(…)       [full reparse]
                └─► fold_walk(root_node, …)         [derive fold regions]
```

### Parsing strategy

The lexer performs a **full reparse** on every `Lex()` call.  No parse tree
is cached between calls — each invocation gets a fresh parse from the current
document content.

This is deliberately simple and robust.  tree-sitter's incremental parsing
API (`ts_tree_edit` + reparse with old tree) requires exact edit descriptors
(start byte, old end byte, new end byte), but Scintilla's `ILexer::Lex()`
interface does not provide this information.  Reconstructing edits from the
length delta is a heuristic that breaks for multi-cursor edits,
find-replace-all, undo/redo of grouped operations, and external file reloads.
A wrong edit descriptor corrupts the old tree and produces incorrect
highlighting that persists.

A full reparse avoids all stale-state bugs.  tree-sitter parses at
10–50 MB/s, so a 100 KB file takes ~5 µs and a 1 MB file well under 1 ms —
imperceptible during interactive editing.

## Limitations & future work

- **32 style slots.**  Scintilla gives custom lexers style IDs 0–255, but
  Notepad++'s Style Configurator practically supports ~32 per language.
  The capture→style mapping is lossy for grammars with many distinct captures.

- **No injection queries.**  Multi-language documents (e.g. HTML with embedded
  JS/CSS) aren't supported yet.  This would require running multiple parsers
  and merging their output.

- **No `locals.scm` support.**  The local-variable scoping queries are loaded
  but not used during highlighting.

## Licence

MIT — see individual files.  tree-sitter itself is MIT-licensed.
The Scintilla/Lexilla headers are under the Scintilla licence.
