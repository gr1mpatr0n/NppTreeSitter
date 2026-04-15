// src/ts_lexer_bridge.cpp
#include "ts_lexer_bridge.h"
#include "scintilla/Scintilla.h"

#include <cstring>
#include <algorithm>
#include <vector>

namespace npp_ts {

// ============================================================================
// Construction / destruction
// ============================================================================

TreeSitterLexer::TreeSitterLexer(const GrammarInfo* grammar,
                                 const StyleMap* styles)
    : grammar_(grammar), styles_(styles), parser_(nullptr), tree_(nullptr)
{
    parser_ = ts_parser_new();
    ts_parser_set_language(parser_, grammar_->language);
}

TreeSitterLexer::~TreeSitterLexer() {
    if (tree_)   ts_tree_delete(tree_);
    if (parser_) ts_parser_delete(parser_);
}

// ============================================================================
// ILexer core
// ============================================================================

int TreeSitterLexer::Version() const { return Scintilla::lvRelease5; } // 3

void TreeSitterLexer::Release() { delete this; }

// ---- Properties (none for now) ----
const char* TreeSitterLexer::PropertyNames()   { return ""; }
int  TreeSitterLexer::PropertyType(const char*) { return 0; }
const char* TreeSitterLexer::DescribeProperty(const char*) { return ""; }
Sci_Position TreeSitterLexer::PropertySet(const char*, const char*) { return 0; }
const char* TreeSitterLexer::DescribeWordListSets() { return ""; }
Sci_Position TreeSitterLexer::WordListSet(int, const char*) { return 0; }

// ============================================================================
// Lex — the main entry point Scintilla calls to style text.
//
// Scintilla tells us "please style bytes [startPos, startPos+lengthDoc)".
// We:
//   1. Obtain the full document text from IDocument.
//   2. Parse (or incrementally re-parse) with tree-sitter.
//   3. Run the highlight query over the requested range.
//   4. Apply styles via IDocument::StartStyling / SetStyleFor.
// ============================================================================

void TreeSitterLexer::full_parse(Scintilla::IDocument* doc) {
    Sci_Position len = doc->Length();
    const char* buf = doc->BufferPointer();

    // Discard the old tree before reparsing.  We pass nullptr as old_tree
    // to force a full reparse.  If we passed the old tree without calling
    // ts_tree_edit() first, tree-sitter would assume nothing changed and
    // return an identical tree — which is why edits weren't being reflected.
    if (tree_) {
        ts_tree_delete(tree_);
        tree_ = nullptr;
    }

    tree_ = ts_parser_parse_string(parser_, nullptr, buf,
                                   static_cast<uint32_t>(len));
}

void TreeSitterLexer::Lex(Sci_PositionU startPos, Sci_Position lengthDoc,
                           int /*initStyle*/, Scintilla::IDocument* pAccess)
{
    std::lock_guard lock(mu_);

    // (Re-)parse the full document.
    // A production version would use TSInputEdit for incremental parsing, but
    // this is correct and adequate for files of typical editing size.
    full_parse(pAccess);

    if (!tree_ || !grammar_->query) {
        // Fallback: style everything as default.
        pAccess->StartStyling(startPos);
        pAccess->SetStyleFor(lengthDoc, 0);
        return;
    }

    uint32_t start = static_cast<uint32_t>(startPos);
    uint32_t end   = static_cast<uint32_t>(startPos + lengthDoc);

    auto ranges = run_highlight_query(tree_, grammar_->query, *styles_,
                                      start, end);

    // Apply styles to the document.
    pAccess->StartStyling(startPos);

    if (ranges.empty()) {
        pAccess->SetStyleFor(lengthDoc, 0);
        return;
    }

    // The query runner returns contiguous, non-overlapping ranges covering
    // [start, end).  Walk them and call SetStyleFor for each run.
    for (auto& r : ranges) {
        pAccess->SetStyleFor(static_cast<Sci_Position>(r.length),
                             static_cast<char>(r.style));
    }
}

// ============================================================================
// Fold — derive fold regions from the syntax tree.
//
// Strategy: every named node whose span crosses at least one line boundary
// opens a fold region.  We walk the tree's immediate children and assign
// fold levels based on depth.
// ============================================================================

static void fold_walk(TSNode node, Scintilla::IDocument* doc, int depth) {
    TSPoint sp = ts_node_start_point(node);
    TSPoint ep = ts_node_end_point(node);

    if (ep.row > sp.row && ts_node_is_named(node)) {
        // This node spans multiple lines — it's a fold header.
        int level = SC_FOLDLEVELBASE + depth;
        level |= SC_FOLDLEVELHEADERFLAG;
        doc->SetLevel(static_cast<Sci_Position>(sp.row), level);

        // Interior lines get level+1.
        for (uint32_t line = sp.row + 1; line <= ep.row; ++line) {
            int inner = SC_FOLDLEVELBASE + depth + 1;
            // Only set if not already set to a higher (more nested) level.
            int existing = doc->GetLevel(static_cast<Sci_Position>(line));
            if ((existing & SC_FOLDLEVELNUMBERMASK) < (inner & SC_FOLDLEVELNUMBERMASK))
                doc->SetLevel(static_cast<Sci_Position>(line), inner);
        }
    }

    uint32_t cc = ts_node_child_count(node);
    for (uint32_t i = 0; i < cc; ++i) {
        fold_walk(ts_node_child(node, i), doc, depth + 1);
    }
}

void TreeSitterLexer::Fold(Sci_PositionU /*startPos*/, Sci_Position /*lengthDoc*/,
                            int /*initStyle*/, Scintilla::IDocument* pAccess)
{
    std::lock_guard lock(mu_);

    if (!tree_) {
        full_parse(pAccess);
    }
    if (!tree_) return;

    // Reset all fold levels to base.
    Sci_Position line_count = pAccess->LineFromPosition(pAccess->Length()) + 1;
    for (Sci_Position i = 0; i < line_count; ++i)
        pAccess->SetLevel(i, SC_FOLDLEVELBASE);

    TSNode root = ts_tree_root_node(tree_);
    fold_walk(root, pAccess, 0);
}

// ============================================================================
// ILexer2–5 stubs
// ============================================================================

void* TreeSitterLexer::PrivateCall(int, void*) { return nullptr; }

int  TreeSitterLexer::LineEndTypesSupported()     { return 0; }
int  TreeSitterLexer::AllocateSubStyles(int, int) { return -1; }
int  TreeSitterLexer::SubStylesStart(int)         { return -1; }
int  TreeSitterLexer::SubStylesLength(int)        { return 0; }
int  TreeSitterLexer::StyleFromSubStyle(int s)    { return s; }
int  TreeSitterLexer::PrimaryStyleFromStyle(int s){ return s; }
void TreeSitterLexer::FreeSubStyles()             {}
void TreeSitterLexer::SetIdentifiers(int, const char*) {}
int  TreeSitterLexer::DistanceToSecondaryStyles() { return 0; }
const char* TreeSitterLexer::GetSubStyleBases()   { return ""; }

int TreeSitterLexer::NamedStyles() {
    return styles_ ? styles_->named_style_count() : 0;
}

const char* TreeSitterLexer::NameOfStyle(int style) {
    return styles_ ? styles_->name_of_style(static_cast<uint8_t>(style))
                   : "default";
}

const char* TreeSitterLexer::TagsOfStyle(int)        { return ""; }
const char* TreeSitterLexer::DescriptionOfStyle(int s) {
    return NameOfStyle(s);
}

// ---- ILexer4 ----
const char* TreeSitterLexer::GetName() {
    // Return the grammar name — this is what appears in Notepad++'s Language
    // menu and in the style XML files.
    return grammar_ ? grammar_->name.c_str() : "treesitter";
}

int TreeSitterLexer::GetIdentifier() {
    // Lexilla uses negative IDs for external lexers.  We pick a value in
    // the external range.  Each grammar would ideally get its own, but
    // for simplicity we hash the name.
    if (!grammar_) return -1;
    int h = 0;
    for (char c : grammar_->name) h = h * 31 + c;
    // Keep it negative and away from built-in IDs.
    return -(std::abs(h) % 10000 + 1000);
}

const char* TreeSitterLexer::PropertyGet(const char*) { return ""; }

} // namespace npp_ts
