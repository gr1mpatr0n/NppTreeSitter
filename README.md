# NppTreeSitter

A Notepad++ plugin that brings [tree-sitter](https://tree-sitter.github.io/tree-sitter/)
based syntax highlighting to Notepad++.

## How it works

NppTreeSitter implements the **ILexer5** interface (Scintilla 5 / Lexilla) and
registers itself as a lexer plugin.  When Notepad++ asks the plugin to style
a document, the plugin:

1. Parses the document text with tree-sitter (full parse; incremental hooks
   are stubbed for a future version).
2. Runs the grammar's `highlights.scm` query to identify capture names
   (e.g. `@keyword`, `@function`, `@string`).
3. Maps each capture name to one of 32 Scintilla style slots via `styles.conf`.
4. Applies the styles through the `IDocument` interface.
5. Derives fold regions from the syntax tree (any named node spanning multiple
   lines becomes a fold header).

The grammars themselves — the `.dll` parser libraries and `.scm` query files —
are loaded at runtime from a config directory, so you can add support for any
language that has a tree-sitter grammar without recompiling the plugin.

## Building

### Prerequisites

- **CMake** ≥ 3.20
- **MSVC** (Visual Studio 2019 or later, with the C++ workload)
- **Git** (for FetchContent to pull tree-sitter)

### Steps

```powershell
# Clone this repository
git clone https://github.com/yourname/npp-treesitter.git
cd npp-treesitter

# Configure (64-bit)
cmake -B build -G "Visual Studio 17 2022" -A x64

# Build
cmake --build build --config Release

# The DLL lands in build/Release/NppTreeSitter.dll
```

For a 32-bit build (if you use 32-bit Notepad++), change `-A x64` to `-A Win32`.

### Using a local tree-sitter checkout

If you've already cloned tree-sitter, pass its path:

```powershell
cmake -B build -DTREESITTER_ROOT="C:/path/to/tree-sitter" ...
```

## Installation

1. Copy `NppTreeSitter.dll` into your Notepad++ `plugins/NppTreeSitter/` directory.

2. Create the config directory and add grammars:

```
%APPDATA%\Notepad++\plugins\config\NppTreeSitter\
├── styles.conf                    # capture → style mapping
└── grammars/
    ├── rust/
    │   ├── grammar.dll            # compiled tree-sitter-rust parser
    │   ├── highlights.scm         # highlight queries
    │   ├── locals.scm             # (optional) local variable queries
    │   ├── extensions.txt         # file extensions, one per line
    │   └── rust.xml               # Notepad++ style definitions
    ├── python/
    │   ├── grammar.dll
    │   ├── highlights.scm
    │   ├── extensions.txt
    │   └── python.xml
    └── ...
```

3. Restart Notepad++.  Each grammar will appear in the **Language** menu.

## Building grammar DLLs

Each tree-sitter grammar repository (e.g. `tree-sitter-rust`, `tree-sitter-python`)
can be compiled into a shared library.  The plugin expects the library to export
a function named `tree_sitter_<language>()`.

### Quick recipe (using tree-sitter CLI)

```bash
# Install the tree-sitter CLI
npm install -g tree-sitter-cli

# Clone a grammar
git clone https://github.com/tree-sitter/tree-sitter-rust.git
cd tree-sitter-rust

# Generate + compile
tree-sitter generate
```

This produces a `tree-sitter-rust.dll` (or `.so`).  Rename it to `grammar.dll`
and drop it into the grammar directory.

### Manual compilation (MSVC)

```powershell
cl /LD /O2 /I tree-sitter/lib/include ^
   src/parser.c src/scanner.c ^
   /Fe:grammar.dll /link /DEF:grammar.def
```

Where `grammar.def` contains:

```def
EXPORTS
    tree_sitter_rust
```

## Configuration

### styles.conf

Maps tree-sitter capture names to Scintilla style IDs (0–31).  The format is:

```ini
# capture_name = style_id
keyword = 1
function = 9
comment = 24
```

Dotted names are resolved with prefix fallback: if `function.builtin` isn't
in the map, `function` is tried.

### Style XML

Notepad++ reads an XML file to determine colours for each style ID.  See
`config/grammars/rust/rust.xml` for an example.  The `fontStyle` attribute
encodes Bold=2, Italic=1, Underline=4 (summed).

Copy the XML to `%APPDATA%\Notepad++\plugins\config\` so the Style Configurator
can find it.

## Architecture

```
plugin_exports.cpp    ← C-linkage DLL entry points (Npp + Lexilla)
plugin_main.cpp       ← Plugin lifecycle, singletons
ts_lexer_bridge.cpp   ← ILexer5 implementation (Lex / Fold)
ts_query_runner.cpp   ← Runs highlights.scm queries, produces styled ranges
style_map.cpp         ← Capture name → style ID mapping
grammar_registry.cpp  ← Discovers & loads grammar DLLs + .scm files
```

### Data flow

```
Notepad++ ──► CreateLexer("rust")
                │
                ▼
          TreeSitterLexer(grammar_info, style_map)
                │
  Scintilla ──► Lex(startPos, length, …, IDocument*)
                │
                ├─► ts_parser_parse_string(…)     [tree-sitter parse]
                ├─► run_highlight_query(…)         [query captures]
                └─► IDocument::StartStyling()      [apply styles]
                    IDocument::SetStyleFor()

  Scintilla ──► Fold(…)
                │
                └─► fold_walk(root_node, …)        [derive fold regions]
```

## Limitations & future work

- **Full reparse on every Lex() call.**  The `TSInputEdit` incremental parsing
  API is not yet wired up.  This is fine for files up to ~100 KB but will be
  slow on very large files.  The infrastructure is in place (`tree_` is kept
  alive between calls); it just needs the edit deltas from `SCN_MODIFIED`.

- **32 style slots.**  Scintilla gives custom lexers style IDs 0–255, but
  Notepad++'s Style Configurator practically supports ~32 per language.
  The capture→style mapping is lossy for grammars with many distinct captures.

- **No injection queries.**  Multi-language documents (e.g. HTML with embedded
  JS/CSS) aren't supported yet.  This would require running multiple parsers
  and merging their output.

- **No `locals.scm` support.**  The local-variable scoping queries are loaded
  but not used during highlighting.

- **Style XML generation.**  Currently you have to write the XML by hand.
  A future version could auto-generate it from `styles.conf`.

## Licence

MIT — see individual files.  tree-sitter itself is MIT-licensed.
The Scintilla/Lexilla headers are under the Scintilla licence.
