// src/ts_lexer_bridge.cpp
//
// Parsing strategy: full reparse on every Lex() call.
//
// Why not incremental?  tree-sitter's incremental API (ts_tree_edit +
// passing old_tree to ts_parser_parse_string) requires *exact* edit
// descriptors: start byte, old end byte, new end byte.  Scintilla's
// ILexer::Lex() does not provide this information — it only gives us
// the region that needs restyling.  Reconstructing edit descriptors from
// the length delta is a heuristic that breaks badly for multi-cursor
// edits, find-replace-all, undo/redo of grouped operations, and
// external file reloads.  A wrong TSInputEdit corrupts the old tree,
// producing incorrect parses that persist until something forces a
// full reparse.
//
// A full reparse is always correct.  tree-sitter parses at 10–50 MB/s,
// so a 100 KB file takes ~5 µs.  Even a 1 MB file takes well under
// 1 ms — imperceptible during interactive editing.
//
// The tree is NOT cached between Lex() calls.  Each call gets a fresh
// parse from the current document content.  This eliminates all stale
// state bugs.

#include "ts_lexer_bridge.h"
#include "scintilla/Scintilla.h"

#include <cstring>
#include <algorithm>
#include <vector>
#include <cstdlib>

namespace npp_ts {

// ============================================================================
// Construction / destruction
// ============================================================================

TreeSitterLexer::TreeSitterLexer(const GrammarInfo* grammar,
                                 const StyleMap* styles)
    : grammar_(grammar), styles_(styles), parser_(nullptr)
{
    parser_ = ts_parser_new();
    ts_parser_set_language(parser_, grammar_->language);
}

TreeSitterLexer::~TreeSitterLexer() {
    if (parser_) ts_parser_delete(parser_);
}

// ============================================================================
// ILexer core
// ============================================================================

int TreeSitterLexer::Version() const { return Scintilla::lvRelease5; }

void TreeSitterLexer::Release() { delete this; }

const char* TreeSitterLexer::PropertyNames()   { return ""; }
int  TreeSitterLexer::PropertyType(const char*) { return 0; }
const char* TreeSitterLexer::DescribeProperty(const char*) { return ""; }
Sci_Position TreeSitterLexer::PropertySet(const char*, const char*) { return 0; }
const char* TreeSitterLexer::DescribeWordListSets() { return ""; }
Sci_Position TreeSitterLexer::WordListSet(int, const char*) { return 0; }

// ============================================================================
// Parsing — always a full, fresh parse.
// ============================================================================

TSTree* TreeSitterLexer::parse(Scintilla::IDocument* doc) {
    uint32_t len = static_cast<uint32_t>(doc->Length());
    const char* buf = doc->BufferPointer();
    return ts_parser_parse_string(parser_, nullptr, buf, len);
}

// ============================================================================
// Lex
// ============================================================================

void TreeSitterLexer::Lex(Sci_PositionU startPos, Sci_Position lengthDoc,
                           int /*initStyle*/, Scintilla::IDocument* pAccess)
{
    std::lock_guard lock(mu_);

    TSTree* tree = parse(pAccess);

    if (!tree || !grammar_->query) {
        pAccess->StartStyling(startPos);
        pAccess->SetStyleFor(lengthDoc, 0);
        if (tree) ts_tree_delete(tree);
        return;
    }

    uint32_t start = static_cast<uint32_t>(startPos);
    uint32_t end   = static_cast<uint32_t>(startPos + lengthDoc);

    auto ranges = run_highlight_query(tree, grammar_->query, *styles_,
                                      start, end);

    pAccess->StartStyling(startPos);

    if (ranges.empty()) {
        pAccess->SetStyleFor(lengthDoc, 0);
    } else {
        for (auto& r : ranges) {
            pAccess->SetStyleFor(static_cast<Sci_Position>(r.length),
                                 static_cast<char>(r.style));
        }
    }

    ts_tree_delete(tree);
}

// ============================================================================
// Fold
// ============================================================================

static void fold_walk(TSNode node, Scintilla::IDocument* doc, int depth) {
    TSPoint sp = ts_node_start_point(node);
    TSPoint ep = ts_node_end_point(node);

    if (ep.row > sp.row && ts_node_is_named(node)) {
        int level = SC_FOLDLEVELBASE + depth;
        level |= SC_FOLDLEVELHEADERFLAG;
        doc->SetLevel(static_cast<Sci_Position>(sp.row), level);

        for (uint32_t line = sp.row + 1; line <= ep.row; ++line) {
            int inner = SC_FOLDLEVELBASE + depth + 1;
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

    TSTree* tree = parse(pAccess);
    if (!tree) return;

    Sci_Position line_count = pAccess->LineFromPosition(pAccess->Length()) + 1;
    for (Sci_Position i = 0; i < line_count; ++i)
        pAccess->SetLevel(i, SC_FOLDLEVELBASE);

    TSNode root = ts_tree_root_node(tree);
    fold_walk(root, pAccess, 0);

    ts_tree_delete(tree);
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

const char* TreeSitterLexer::GetName() {
    return grammar_ ? grammar_->name.c_str() : "treesitter";
}

int TreeSitterLexer::GetIdentifier() {
    if (!grammar_) return -1;
    int h = 0;
    for (char c : grammar_->name) h = h * 31 + c;
    return -(std::abs(h) % 10000 + 1000);
}

const char* TreeSitterLexer::PropertyGet(const char*) { return ""; }

} // namespace npp_ts
