# NppTreeSitter — Full Build Flow (Arch Linux / MinGW-w64)

This document walks through cross-compiling the plugin and the Zig grammar
DLL from Arch Linux using the MinGW-w64 toolchain, then deploying to a
Windows machine running Notepad++.

---

## 0. Prerequisites (Arch)

```bash
sudo pacman -S mingw-w64-gcc cmake git
```

This installs:
- `x86_64-w64-mingw32-gcc` / `g++` — the cross-compiler targeting Windows x64
- `cmake` — build system
- `git` — for cloning tree-sitter and grammar repos

If you target 32-bit Notepad++, install `mingw-w64-gcc` and use the
`i686-w64-mingw32-` prefix variant (and adjust the toolchain file).

---

## 1. Quick Path — One Command

The included `Makefile` automates the entire flow:

```bash
cd npp-treesitter
make all
```

This will:
1. Fetch tree-sitter v0.24.7 via CMake FetchContent
2. Cross-compile the plugin DLL (`NppTreeSitter.dll`)
3. Clone `tree-sitter-grammars/tree-sitter-zig` at v1.1.2
4. Cross-compile the Zig grammar DLL (`grammar.dll`)
5. Assemble a deployment package in `build/package/`

The output:

```
build/package/
├── plugins/
│   └── NppTreeSitter/
│       └── NppTreeSitter.dll
└── config/
    └── NppTreeSitter/
        ├── styles.conf
        └── grammars/
            └── zig/
                ├── grammar.dll
                ├── highlights.scm
                └── extensions.txt
```

Skip to **§6. Deploy on Windows** below.

---

## 2. Step-by-Step: Build the Plugin DLL

If you prefer to drive each step manually:

```bash
cd npp-treesitter

# Configure with the MinGW toolchain file.
cmake -B build/plugin \
      -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake \
      -DCMAKE_BUILD_TYPE=Release

# Build (parallel).
cmake --build build/plugin -j$(nproc)
```

Output: `build/plugin/NppTreeSitter.dll`

The CMake build uses FetchContent to pull tree-sitter v0.24.7 automatically,
compiles it as a static library (`lib.c` unity build), and links it into the
plugin DLL.  The resulting DLL has no runtime dependencies beyond Windows
system libraries (`kernel32`, `user32`, `shlwapi`).

---

## 3. Step-by-Step: Build the Zig Grammar DLL

### 3a. Clone the sources

```bash
# tree-sitter core (for headers + lib.c)
git clone --depth 1 --branch v0.24.7 \
    https://github.com/tree-sitter/tree-sitter.git \
    build/tree-sitter

# Zig grammar
git clone --depth 1 --branch v1.1.2 \
    https://github.com/tree-sitter-grammars/tree-sitter-zig.git \
    build/grammars/tree-sitter-zig
```

### 3b. Cross-compile

```bash
mkdir -p build/grammars/out/zig

x86_64-w64-mingw32-gcc -shared -O2 -DNDEBUG \
    -I build/tree-sitter/lib/include \
    -I build/tree-sitter/lib/src \
    build/grammars/tree-sitter-zig/src/parser.c \
    build/tree-sitter/lib/src/lib.c \
    -o build/grammars/out/zig/grammar.dll \
    -Wl,--export-all-symbols \
    -static-libgcc
```

That's it — one `gcc` invocation.  The tree-sitter-zig grammar has no
external scanner, so `parser.c` is the only grammar source file.

### 3c. Verify the export

```bash
x86_64-w64-mingw32-objdump -p build/grammars/out/zig/grammar.dll \
    | grep -i "tree_sitter_zig"
```

You should see `tree_sitter_zig` in the export table.

---

## 4. Grammars with External Scanners

Some grammars (e.g. Python, HTML) have a `src/scanner.c` (C) or
`src/scanner.cc` (C++).  For those, add the scanner to the compile command:

```bash
# Example: Python (has scanner.c)
x86_64-w64-mingw32-gcc -shared -O2 -DNDEBUG \
    -I build/tree-sitter/lib/include \
    -I build/tree-sitter/lib/src \
    build/grammars/tree-sitter-python/src/parser.c \
    build/grammars/tree-sitter-python/src/scanner.c \
    build/tree-sitter/lib/src/lib.c \
    -o build/grammars/out/python/grammar.dll \
    -Wl,--export-all-symbols \
    -static-libgcc
```

For C++ scanners (`scanner.cc`), use `g++` instead:

```bash
# Example: hypothetical grammar with C++ scanner
x86_64-w64-mingw32-g++ -shared -O2 -DNDEBUG \
    -I build/tree-sitter/lib/include \
    -I build/tree-sitter/lib/src \
    build/grammars/tree-sitter-foo/src/parser.c \
    build/grammars/tree-sitter-foo/src/scanner.cc \
    build/tree-sitter/lib/src/lib.c \
    -o build/grammars/out/foo/grammar.dll \
    -Wl,--export-all-symbols \
    -static-libgcc -static-libstdc++
```

The generic Makefile target handles scanner detection automatically:

```bash
make grammar NAME=python
make grammar NAME=c
make grammar NAME=javascript
```

---

## 5. Prepare the Config Files

### 5a. extensions.txt

One file extension per line (including the dot):

```
.zig
.zon
```

### 5b. highlights.scm

Copy directly from the grammar repo:

```bash
cp build/grammars/tree-sitter-zig/queries/highlights.scm \
   <deploy>/config/NppTreeSitter/grammars/zig/
```

### 5c. styles.conf (global)

The default `config/styles.conf` in the repo maps all common tree-sitter
capture names to Scintilla style IDs 0–31.  Copy it once:

```bash
cp config/styles.conf <deploy>/config/NppTreeSitter/
```

### 5d. Style XML (optional, per-language)

See `config/grammars/rust/rust.xml` for a template.  This controls colours
in Notepad++'s Style Configurator.  Without it, the plugin still works but
the default colour scheme applies.

---

## 6. Deploy on Windows

Copy the package tree onto the Windows machine:

```
# Plugin DLL → Notepad++ install directory
build/package/plugins/NppTreeSitter/NppTreeSitter.dll
    → C:\Program Files\Notepad++\plugins\NppTreeSitter\NppTreeSitter.dll

# Config → Notepad++ appdata config directory
build/package/config/NppTreeSitter\
    → %APPDATA%\Notepad++\plugins\config\NppTreeSitter\
```

For portable Notepad++ installs, both go under the Notepad++ directory.

Restart Notepad++.  **Language → zig** should appear in the menu.

---

## 7. Test

Create or open a `.zig` file:

```zig
const std = @import("std");

pub fn main() !void {
    const stdout = std.io.getStdOut().writer();
    try stdout.print("Hello, {s}!\n", .{"world"});
}

test "simple test" {
    var x: i32 = 42;
    x += 1;
    try std.testing.expectEqual(@as(i32, 43), x);
}
```

Select **Language → zig**.  Keywords should highlight in blue, strings in
orange, comments in green, etc.

---

## 8. Troubleshooting

### Plugin DLL doesn't load

- **Architecture mismatch:** 64-bit Notepad++ needs a DLL built with
  `x86_64-w64-mingw32-gcc`.  32-bit Notepad++ needs `i686-w64-mingw32-gcc`.

- **Missing runtime dependencies:** The Makefile passes `-static-libgcc`
  and `-static-libstdc++` to avoid depending on `libgcc_s_seh-1.dll` or
  `libstdc++-6.dll`.  If you built manually, check with:

  ```bash
  x86_64-w64-mingw32-objdump -p NppTreeSitter.dll | grep "DLL Name"
  ```

  You should see only `KERNEL32.dll`, `USER32.dll`, `SHLWAPI.dll`, and
  similar Windows system DLLs.

### "zig" doesn't appear in the Language menu

- Verify `grammar.dll` is in the right config directory and exports
  `tree_sitter_zig`.

- Verify `highlights.scm` and `extensions.txt` are alongside `grammar.dll`.

- Open **Plugins → About NppTreeSitter** to confirm the plugin loaded.

### Query compilation errors at runtime

- Make sure `highlights.scm` comes from the **same tag** as the parser
  (`v1.1.2`).  Mismatched query/grammar versions cause "invalid node type"
  errors.

- Some upstream `highlights.scm` files use editor-specific predicates (e.g.
  Neovim's `#lua-match?`).  The plugin uses the tree-sitter C API which
  only supports `#match?`, `#eq?`, `#any-of?`, and `#is?`.  Use the
  grammar repo's own `queries/highlights.scm`, not nvim-treesitter's.

---

## Appendix: Adding More Grammars

```bash
# Build any grammar with one command:
make grammar NAME=c
make grammar NAME=python
make grammar NAME=javascript
make grammar NAME=rust
make grammar NAME=go

# Then for each, create the config directory on Windows:
#   grammars/<name>/grammar.dll
#   grammars/<name>/highlights.scm    (from the repo's queries/)
#   grammars/<name>/extensions.txt    (one extension per line)
```
