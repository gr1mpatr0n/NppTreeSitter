// src/ts_lexer_bridge.cpp
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
    : grammar_(grammar), styles_(styles),
      parser_(nullptr), tree_(nullptr), prev_doc_len_(0)
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

int TreeSitterLexer::Version() const { return Scintilla::lvRelease5; }

void TreeSitterLexer::Release() { delete this; }

const char* TreeSitterLexer::PropertyNames()   { return ""; }
int  TreeSitterLexer::PropertyType(const char*) { return 0; }
const char* TreeSitterLexer::DescribeProperty(const char*) { return ""; }
Sci_Position TreeSitterLexer::PropertySet(const char*, const char*) { return 0; }
const char* TreeSitterLexer::DescribeWordListSets() { return ""; }
Sci_Position TreeSitterLexer::WordListSet(int, const char*) { return 0; }

// ============================================================================
// Incremental parsing
//
// tree-sitter's incremental parsing requires:
//   1. Call ts_tree_edit() on the OLD tree with a TSInputEdit describing
//      the change (start byte, old end byte, new end byte, plus row/col).
//   2. Pass the edited old tree to ts_parser_parse_string().
//   3. tree-sitter diffs the old and new trees internally and only
//      reparses the affected region.
//
// The challenge is that Scintilla's ILexer::Lex() does not receive
// explicit edit deltas.  We reconstruct them:
//
//   - We know the old document length (prev_doc_len_) from the last parse.
//   - We know the new document length from IDocument::Length().
//   - Scintilla's startPos parameter is the beginning of the line where
//     the edit occurred — a good approximation for the edit start byte.
//   - The net change in bytes is (new_len - old_len).
//
// For a single insertion of N bytes at position P:
//   start_byte  = P
//   old_end_byte = P        (nothing was there before)
//   new_end_byte = P + N
//
// For a single deletion of N bytes at position P:
//   start_byte  = P
//   old_end_byte = P + N    (N bytes were there before)
//   new_end_byte = P        (they're gone now)
//
// This heuristic handles the common case of typing / deleting at a single
// point.  For multi-cursor or find-replace-all, the heuristic is inexact
// but tree-sitter is resilient — in the worst case it reparses more than
// strictly necessary, which is correct but not maximally efficient.
//
// If there is no old tree (first parse), we skip the edit step entirely.
// ============================================================================

TSPoint TreeSitterLexer::byte_to_point(Scintilla::IDocument* doc,
                                       uint32_t byte_offset)
{
    Sci_Position line = doc->LineFromPosition(static_cast<Sci_Position>(byte_offset));
    Sci_Position line_start = doc->LineStart(line);
    return TSPoint{
        static_cast<uint32_t>(line),
        static_cast<uint32_t>(byte_offset - static_cast<uint32_t>(line_start))
    };
}

void TreeSitterLexer::parse(Scintilla::IDocument* doc, uint32_t edit_pos) {
    uint32_t new_len = static_cast<uint32_t>(doc->Length());
    const char* buf = doc->BufferPointer();

    if (tree_ && prev_doc_len_ != new_len) {
        // Compute the edit delta and apply it to the old tree.
        int32_t delta = static_cast<int32_t>(new_len) -
                        static_cast<int32_t>(prev_doc_len_);

        uint32_t start_byte = edit_pos;
        uint32_t old_end_byte;
        uint32_t new_end_byte;

        if (delta >= 0) {
            // Insertion: `delta` bytes were inserted at start_byte.
            old_end_byte = start_byte;
            new_end_byte = start_byte + static_cast<uint32_t>(delta);
        } else {
            // Deletion: `|delta|` bytes were removed starting at start_byte.
            old_end_byte = start_byte + static_cast<uint32_t>(-delta);
            new_end_byte = start_byte;
        }

        // Clamp to valid ranges.
        if (old_end_byte > prev_doc_len_)
            old_end_byte = prev_doc_len_;
        if (new_end_byte > new_len)
            new_end_byte = new_len;

        // Build the TSInputEdit.  For the point (row, col) fields we
        // use the post-edit document for start and new_end, and
        // approximate old_end from the old tree's root node.
        TSPoint start_point = byte_to_point(doc, start_byte);
        TSPoint new_end_point = byte_to_point(doc, new_end_byte);

        // For old_end_point, we can't query the old document (it's gone),
        // so we use the old tree's root end point if old_end_byte was at
        // or past the old document end, otherwise approximate from the
        // new document (which is close enough for tree-sitter's purposes).
        TSPoint old_end_point;
        if (old_end_byte >= prev_doc_len_) {
            TSNode root = ts_tree_root_node(tree_);
            old_end_point = ts_node_end_point(root);
        } else {
            // Approximate: use the start_point row + column offset.
            // This isn't perfect but tree-sitter only uses the point for
            // row/column in error reporting, not for parse correctness.
            old_end_point = start_point;
            if (delta < 0) {
                old_end_point.column += static_cast<uint32_t>(-delta);
            }
        }

        TSInputEdit edit{
            start_byte,
            old_end_byte,
            new_end_byte,
            start_point,
            old_end_point,
            new_end_point
        };

        ts_tree_edit(tree_, &edit);
    }

    // Parse: if tree_ is non-null (and was just edited), tree-sitter does
    // an incremental reparse.  If tree_ is null (first parse), it does a
    // full parse.
    TSTree* new_tree = ts_parser_parse_string(parser_, tree_, buf, new_len);

    if (tree_) ts_tree_delete(tree_);
    tree_ = new_tree;
    prev_doc_len_ = new_len;
}

// ============================================================================
// Lex
// ============================================================================

void TreeSitterLexer::Lex(Sci_PositionU startPos, Sci_Position lengthDoc,
                           int /*initStyle*/, Scintilla::IDocument* pAccess)
{
    std::lock_guard lock(mu_);

    parse(pAccess, static_cast<uint32_t>(startPos));

    if (!tree_ || !grammar_->query) {
        pAccess->StartStyling(startPos);
        pAccess->SetStyleFor(lengthDoc, 0);
        return;
    }

    uint32_t start = static_cast<uint32_t>(startPos);
    uint32_t end   = static_cast<uint32_t>(startPos + lengthDoc);

    auto ranges = run_highlight_query(tree_, grammar_->query, *styles_,
                                      start, end);

    pAccess->StartStyling(startPos);

    if (ranges.empty()) {
        pAccess->SetStyleFor(lengthDoc, 0);
        return;
    }

    for (auto& r : ranges) {
        pAccess->SetStyleFor(static_cast<Sci_Position>(r.length),
                             static_cast<char>(r.style));
    }
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

    if (!tree_) {
        parse(pAccess, 0);
    }
    if (!tree_) return;

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
