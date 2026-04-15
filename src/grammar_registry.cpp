// src/grammar_registry.cpp
#include "grammar_registry.h"

#include <fstream>
#include <sstream>
#include <filesystem>
#include <cstring>

#include <windows.h>

namespace fs = std::filesystem;

namespace npp_ts {

GrammarInfo& GrammarInfo::operator=(GrammarInfo&& o) noexcept {
    if (this != &o) {
        if (query) ts_query_delete(query);
        name       = std::move(o.name);
        extensions = std::move(o.extensions);
        language   = o.language;   o.language = nullptr;
        highlights = std::move(o.highlights);
        locals     = std::move(o.locals);
        query      = o.query;      o.query = nullptr;
    }
    return *this;
}

// ---------------------------------------------------------------------------
// File helpers
// ---------------------------------------------------------------------------

static std::string read_file(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) return {};
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static std::vector<std::string> read_extensions(const std::string& dir) {
    // Read a simple "extensions.txt" listing one extension per line,
    // e.g. ".rs\n" or ".c\n.h\n".
    auto path = (fs::path(dir) / "extensions.txt").string();
    std::vector<std::string> exts;
    std::ifstream f(path);
    if (!f.is_open()) return exts;
    std::string line;
    while (std::getline(f, line)) {
        // Trim whitespace.
        while (!line.empty() && (line.back() == '\r' || line.back() == ' '))
            line.pop_back();
        if (!line.empty()) {
            if (line.front() != '.') line.insert(line.begin(), '.');
            exts.push_back(line);
        }
    }
    return exts;
}

// ---------------------------------------------------------------------------
// Dynamic library loading
// ---------------------------------------------------------------------------

static TSLanguageFn load_language_fn(const std::string& dll_path,
                                     const std::string& grammar_name)
{
    // The symbol name is tree_sitter_<name>, e.g. tree_sitter_rust.
    std::string sym = "tree_sitter_" + grammar_name;

    HMODULE h = LoadLibraryA(dll_path.c_str());
    if (!h) return nullptr;
    auto fn = reinterpret_cast<TSLanguageFn>(GetProcAddress(h, sym.c_str()));
    // We intentionally never FreeLibrary — the grammar stays loaded for the
    // lifetime of Notepad++.
    return fn;
}

// ---------------------------------------------------------------------------
// Query compilation
// ---------------------------------------------------------------------------

bool GrammarRegistry::compile_query(GrammarInfo& info) {
    if (info.highlights.empty() || !info.language) return false;

    uint32_t error_offset = 0;
    TSQueryError error_type = TSQueryErrorNone;

    info.query = ts_query_new(
        info.language,
        info.highlights.c_str(),
        static_cast<uint32_t>(info.highlights.size()),
        &error_offset,
        &error_type
    );

    if (!info.query) {
        // TODO: log error_offset and error_type somewhere visible.
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Loading a single grammar directory
// ---------------------------------------------------------------------------

bool GrammarRegistry::load_grammar_dir(const std::string& dir,
                                       const std::string& name)
{
    // Always .dll — this plugin targets Windows (Notepad++ is Windows-only).
    std::string dll = (fs::path(dir) / "grammar.dll").string();

    if (!fs::exists(dll)) return false;

    auto lang_fn = load_language_fn(dll, name);
    if (!lang_fn) return false;

    auto info = std::make_unique<GrammarInfo>();
    info->name       = name;
    info->language   = lang_fn();
    info->highlights = read_file((fs::path(dir) / "highlights.scm").string());
    info->locals     = read_file((fs::path(dir) / "locals.scm").string());
    info->extensions = read_extensions(dir);

    if (info->highlights.empty()) return false;
    if (!compile_query(*info)) return false;

    // Register extension associations.
    for (auto& ext : info->extensions)
        by_ext_[ext] = info.get();

    by_name_[name] = std::move(info);
    return true;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

bool GrammarRegistry::scan(const std::string& config_dir) {
    auto grammars_dir = fs::path(config_dir) / "grammars";
    if (!fs::is_directory(grammars_dir)) return false;

    bool any = false;
    for (auto& entry : fs::directory_iterator(grammars_dir)) {
        if (!entry.is_directory()) continue;
        std::string name = entry.path().filename().string();
        if (load_grammar_dir(entry.path().string(), name))
            any = true;
    }
    return any;
}

bool GrammarRegistry::register_builtin(const std::string& name,
                                       const TSLanguage* lang,
                                       const std::string& highlights_scm,
                                       const std::vector<std::string>& extensions)
{
    auto info = std::make_unique<GrammarInfo>();
    info->name       = name;
    info->language   = lang;
    info->highlights = highlights_scm;
    info->extensions = extensions;

    if (!compile_query(*info)) return false;

    for (auto& ext : extensions)
        by_ext_[ext] = info.get();

    by_name_[name] = std::move(info);
    return true;
}

const GrammarInfo* GrammarRegistry::find_by_extension(const std::string& ext) const {
    auto it = by_ext_.find(ext);
    return (it != by_ext_.end()) ? it->second : nullptr;
}

const GrammarInfo* GrammarRegistry::find_by_name(const std::string& name) const {
    auto it = by_name_.find(name);
    return (it != by_name_.end()) ? it->second.get() : nullptr;
}

std::vector<std::string> GrammarRegistry::names() const {
    std::vector<std::string> out;
    out.reserve(by_name_.size());
    for (auto& [k, _] : by_name_) out.push_back(k);
    return out;
}

} // namespace npp_ts
