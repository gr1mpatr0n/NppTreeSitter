// src/ts_lexer_bridge.h
// ILexer5 implementation that delegates to tree-sitter for lexing/folding.
//
// Parsing strategy: full reparse on every Lex() call.  tree-sitter parses
// at tens of MB/s so this is sub-millisecond for typical editing files.
// A full reparse is always correct regardless of edit pattern (multi-cursor,
// find-replace-all, undo/redo, external file changes, etc.).
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
    std::mutex          mu_;

    /// Full-parse the document and return the tree.  Caller owns it.
    TSTree* parse(Scintilla::IDocument* doc);
};

} // namespace npp_ts
