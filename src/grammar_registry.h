// src/grammar_registry.h
// Manages the set of available tree-sitter grammars (language parsers)
// and their associated highlights.scm query files.
#pragma once

#include <string>
#include <unordered_map>
#include <memory>
#include <vector>
#include <functional>

#include <tree_sitter/api.h>

namespace npp_ts {

/// Information about a single registered grammar.
struct GrammarInfo {
    std::string                name;         // e.g. "rust", "c", "python"
    std::vector<std::string>   extensions;   // e.g. {".rs"}, {".c", ".h"}
    const TSLanguage*          language;      // from the grammar shared library
    std::string                highlights;   // contents of highlights.scm
    std::string                locals;       // contents of locals.scm (optional)
    TSQuery*                   query;        // compiled highlight query (owned)

    ~GrammarInfo() {
        if (query) ts_query_delete(query);
    }

    // Non-copyable, movable.
    GrammarInfo() : language(nullptr), query(nullptr) {}
    GrammarInfo(GrammarInfo&& o) noexcept
        : name(std::move(o.name)), extensions(std::move(o.extensions)),
          language(o.language), highlights(std::move(o.highlights)),
          locals(std::move(o.locals)), query(o.query)
    { o.query = nullptr; o.language = nullptr; }
    GrammarInfo& operator=(GrammarInfo&&) noexcept;
    GrammarInfo(const GrammarInfo&) = delete;
    GrammarInfo& operator=(const GrammarInfo&) = delete;
};

/// The type of a tree_sitter_<lang>() function exported by grammar DLLs.
using TSLanguageFn = const TSLanguage* (*)();

/// Registry that discovers and loads grammars from a config directory.
///
/// Expected directory layout under `config_dir`:
///
///   grammars/
///     rust/
///       grammar.dll          (or .so — exports tree_sitter_rust)
///       highlights.scm
///       locals.scm           (optional)
///     python/
///       grammar.dll
///       highlights.scm
///     …
///
class GrammarRegistry {
public:
    /// Scan `config_dir/grammars/` and load every grammar found.
    /// `config_dir` is typically the plugin's own config directory.
    bool scan(const std::string& config_dir);

    /// Register a grammar whose TSLanguage* is provided directly
    /// (useful for statically linked grammars in testing).
    bool register_builtin(const std::string& name,
                          const TSLanguage* lang,
                          const std::string& highlights_scm,
                          const std::vector<std::string>& extensions);

    /// Look up a grammar by file extension (including the dot).
    const GrammarInfo* find_by_extension(const std::string& ext) const;

    /// Look up by name.
    const GrammarInfo* find_by_name(const std::string& name) const;

    /// List all registered grammar names.
    std::vector<std::string> names() const;

private:
    std::unordered_map<std::string, std::unique_ptr<GrammarInfo>> by_name_;
    std::unordered_map<std::string, GrammarInfo*>                 by_ext_;

    bool load_grammar_dir(const std::string& dir, const std::string& name);
    bool compile_query(GrammarInfo& info);
};

} // namespace npp_ts
