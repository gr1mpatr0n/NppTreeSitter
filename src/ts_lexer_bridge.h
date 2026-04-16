// src/ts_lexer_bridge.h
// ILexer5 implementation that delegates to tree-sitter for lexing/folding.
//
// Supports incremental parsing: on subsequent Lex() calls after the initial
// parse, we compute a TSInputEdit from the difference between the old tree's
// size and the current document, apply it to the old tree, then reparse.
// tree-sitter only reparses the affected region.
#pragma once

#include "scintilla/ILexer.h"
#include "grammar_registry.h"
#include "style_map.h"
#include "ts_query_runner.h"

#include <tree_sitter/api.h>
#include <string>
#include <mutex>

namespace npp_ts {

class TreeSitterLexer : public Scintilla::ILexer5 {
public:
    explicit TreeSitterLexer(const GrammarInfo* grammar, const StyleMap* styles);
    ~TreeSitterLexer();

    TreeSitterLexer(const TreeSitterLexer&) = delete;
    TreeSitterLexer& operator=(const TreeSitterLexer&) = delete;

    // ---- ILexer4 (base) ----
    int SCI_METHOD Version() const override;
    void SCI_METHOD Release() override;

    const char * SCI_METHOD PropertyNames() override;
    int SCI_METHOD PropertyType(const char *name) override;
    const char * SCI_METHOD DescribeProperty(const char *name) override;
    Sci_Position SCI_METHOD PropertySet(const char *key, const char *val) override;
    const char * SCI_METHOD DescribeWordListSets() override;
    Sci_Position SCI_METHOD WordListSet(int n, const char *wl) override;

    void SCI_METHOD Lex(Sci_PositionU startPos, Sci_Position lengthDoc,
                        int initStyle, Scintilla::IDocument *pAccess) override;
    void SCI_METHOD Fold(Sci_PositionU startPos, Sci_Position lengthDoc,
                         int initStyle, Scintilla::IDocument *pAccess) override;

    void * SCI_METHOD PrivateCall(int operation, void *pointer) override;

    int SCI_METHOD LineEndTypesSupported() override;
    int SCI_METHOD AllocateSubStyles(int styleBase, int numberStyles) override;
    int SCI_METHOD SubStylesStart(int styleBase) override;
    int SCI_METHOD SubStylesLength(int styleBase) override;
    int SCI_METHOD StyleFromSubStyle(int subStyle) override;
    int SCI_METHOD PrimaryStyleFromStyle(int style) override;
    void SCI_METHOD FreeSubStyles() override;
    void SCI_METHOD SetIdentifiers(int style, const char *identifiers) override;
    int SCI_METHOD DistanceToSecondaryStyles() override;
    const char * SCI_METHOD GetSubStyleBases() override;
    int SCI_METHOD NamedStyles() override;
    const char * SCI_METHOD NameOfStyle(int style) override;
    const char * SCI_METHOD TagsOfStyle(int style) override;
    const char * SCI_METHOD DescriptionOfStyle(int style) override;

    // ---- ILexer5 (additions) ----
    const char * SCI_METHOD GetName() override;
    int SCI_METHOD GetIdentifier() override;
    const char * SCI_METHOD PropertyGet(const char *key) override;

private:
    const GrammarInfo*  grammar_;
    const StyleMap*     styles_;

    TSParser*           parser_;
    TSTree*             tree_;
    std::mutex          mu_;

    // The document length as of the last successful parse.
    // Used to detect insertions/deletions for incremental parsing.
    uint32_t            prev_doc_len_ = 0;

    /// Parse the document, incrementally if possible.
    /// `edit_pos` is a hint for where the edit occurred (typically startPos
    /// from the Lex() call).
    void parse(Scintilla::IDocument* doc, uint32_t edit_pos);

    /// Byte offset → TSPoint using IDocument line mapping.
    static TSPoint byte_to_point(Scintilla::IDocument* doc, uint32_t byte_offset);
};

} // namespace npp_ts
