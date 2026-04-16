#!/usr/bin/env python3
"""Generate the final NSIS installer script with grammar sections.

Reads installer/NppTreeSitter.nsi as a template, discovers grammars from
the package directory, and writes the completed script to stdout (or a
specified output file).

Usage:
    python3 installer/generate_nsi.py build/package > build/NppTreeSitter.nsi
    python3 installer/generate_nsi.py build/package build/NppTreeSitter.nsi
"""

import os
import sys
from pathlib import Path


def main():
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <package_dir> [output_file]", file=sys.stderr)
        sys.exit(1)

    package_dir = Path(sys.argv[1])
    output_file = sys.argv[2] if len(sys.argv) > 2 else None

    template_path = Path(__file__).parent / "NppTreeSitter.nsi"
    template = template_path.read_text()

    grammars_dir = package_dir / "config" / "NppTreeSitter" / "grammars"
    if not grammars_dir.is_dir():
        print(f"No grammars directory at {grammars_dir}", file=sys.stderr)
        sys.exit(1)

    # Discover grammars
    grammars = sorted(
        d.name for d in grammars_dir.iterdir()
        if d.is_dir() and (d / "grammar.dll").exists()
    )

    if not grammars:
        print("No grammar DLLs found in package", file=sys.stderr)
        sys.exit(1)

    # Build NSIS section blocks
    sections = []
    descriptions = []

    for lang in grammars:
        sec_id = f"SecGrammar_{lang}"
        grammar_path = grammars_dir / lang

        # Read extensions for description
        exts_file = grammar_path / "extensions.txt"
        exts = ""
        if exts_file.exists():
            exts = " ".join(
                line.strip().lstrip(".")
                for line in exts_file.read_text().splitlines()
                if line.strip()
            )

        # List all files in the grammar directory
        files = sorted(f.name for f in grammar_path.iterdir() if f.is_file())

        section_lines = [
            f'Section "{lang}" {sec_id}',
            f'    SetOutPath "$APPDATA\\Notepad++\\plugins\\config\\NppTreeSitter\\grammars\\{lang}"',
        ]
        for fname in files:
            section_lines.append(
                f'    File "${{PACKAGE_DIR}}\\config\\NppTreeSitter\\grammars\\{lang}\\{fname}"'
            )
        section_lines.append("SectionEnd")
        sections.append("\n".join(section_lines))

        desc = f"Tree-sitter grammar for {lang}"
        if exts:
            desc += f" ({exts})"
        descriptions.append(
            f'    !insertmacro MUI_DESCRIPTION_TEXT ${{{sec_id}}} "{desc}"'
        )

    sections_text = "\n\n".join(sections)
    descriptions_text = "\n".join(descriptions)

    output = template.replace(";GRAMMAR_SECTIONS_MARKER", sections_text)
    output = output.replace(";GRAMMAR_DESCRIPTIONS_MARKER", descriptions_text)

    if output_file:
        Path(output_file).write_text(output)
        print(f"Generated {output_file} with {len(grammars)} grammars: {', '.join(grammars)}")
    else:
        print(output)


if __name__ == "__main__":
    main()
