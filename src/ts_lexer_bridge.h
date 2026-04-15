// src/ts_lexer_bridge.h
// ILexer5 implementation that delegates to tree-sitter for lexing/folding.
#pragma once

#include "scintilla/ILexer.h"
#include "grammar_registry.h"
#include "style_map.h"
#include "ts_query_runner.h"

#include <tree_sitter/api.h>
#include <string>
#include <mutex>

namespace npp_ts {

/// A Scintilla ILexer5 implementation backed by tree-sitter.
///
/// One instance is created per "language" (grammar) via CreateLexer().
/// Notepad++ calls Lex() and Fold() on it whenever re-styling is needed.
class TreeSitterLexer : public Scintilla::ILexer5 {
public:
    explicit TreeSitterLexer(const GrammarInfo* grammar, const StyleMap* styles);
    ~TreeSitterLexer();

    // Non-copyable.
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
    TSTree*             tree_;      // incrementally maintained
    std::mutex          mu_;       // protects tree_ / parser_

    // Helper: full-parse from IDocument text.
    void full_parse(Scintilla::IDocument* doc);
};

} // namespace npp_ts
