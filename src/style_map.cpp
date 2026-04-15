// src/style_map.cpp
#include "style_map.h"

#include <fstream>
#include <sstream>
#include <algorithm>

namespace npp_ts {

StyleMap::StyleMap() {
    load_defaults();
}

void StyleMap::load_defaults() {
    // We use Scintilla style slots 0–31.
    // Slot 0 = default / unmapped text.
    // The rest map to common tree-sitter highlight capture groups.
    // These match the "standard" capture names used by nvim-treesitter and
    // most grammar repositories' highlights.scm files.

    struct Default { uint8_t id; const char* name; };
    static constexpr Default defaults[] = {
        { 0, "default"              },
        { 1, "keyword"              },
        { 2, "keyword.function"     },
        { 3, "keyword.return"       },
        { 4, "keyword.operator"     },
        { 5, "string"               },
        { 6, "string.special"       },
        { 7, "number"               },
        { 8, "float"                },
        { 9, "function"             },
        {10, "function.builtin"     },
        {11, "function.call"        },
        {12, "method"               },
        {13, "method.call"          },
        {14, "type"                 },
        {15, "type.builtin"         },
        {16, "type.qualifier"       },
        {17, "variable"             },
        {18, "variable.builtin"     },
        {19, "variable.parameter"   },
        {20, "property"             },
        {21, "field"                },
        {22, "constant"             },
        {23, "constant.builtin"     },
        {24, "comment"              },
        {25, "operator"             },
        {26, "punctuation"          },
        {27, "punctuation.bracket"  },
        {28, "punctuation.delimiter"},
        {29, "label"                },
        {30, "attribute"            },
        {31, "constructor"          },
    };

    styles_.clear();
    styles_.resize(32);
    capture_to_style_.clear();

    for (auto& d : defaults) {
        styles_[d.id] = {d.id, d.name};
        capture_to_style_[d.name] = d.id;
    }

    default_style_ = 0;
}

bool StyleMap::load_from_file(const std::string& path) {
    // Simple line-based config format:
    //   # comment
    //   capture_name = style_number
    //
    // e.g.:
    //   keyword = 1
    //   function.builtin = 10

    std::ifstream f(path);
    if (!f.is_open()) return false;

    styles_.clear();
    styles_.resize(32);
    capture_to_style_.clear();

    // Pre-fill style names.
    for (uint8_t i = 0; i < 32; ++i) {
        styles_[i] = {i, ""};
    }

    std::string line;
    while (std::getline(f, line)) {
        // Strip comments and whitespace.
        auto hash = line.find('#');
        if (hash != std::string::npos) line.resize(hash);
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string name = line.substr(0, eq);
        std::string val  = line.substr(eq + 1);

        // Trim.
        auto trim = [](std::string& s) {
            while (!s.empty() && s.front() == ' ') s.erase(s.begin());
            while (!s.empty() && s.back()  == ' ') s.pop_back();
        };
        trim(name);
        trim(val);

        int id = 0;
        try { id = std::stoi(val); } catch (...) { continue; }
        if (id < 0 || id > 31) continue;

        uint8_t uid = static_cast<uint8_t>(id);
        capture_to_style_[name] = uid;
        styles_[uid] = {uid, name};
    }

    return true;
}

uint8_t StyleMap::style_for_capture(std::string_view capture) const {
    // Exact match first.
    std::string key(capture);
    auto it = capture_to_style_.find(key);
    if (it != capture_to_style_.end()) return it->second;

    // Prefix fallback: "function.builtin" → try "function".
    while (true) {
        auto dot = key.rfind('.');
        if (dot == std::string::npos) break;
        key.resize(dot);
        it = capture_to_style_.find(key);
        if (it != capture_to_style_.end()) return it->second;
    }

    return default_style_;
}

const char* StyleMap::name_of_style(uint8_t style) const {
    if (style < styles_.size() && !styles_[style].capture_name.empty())
        return styles_[style].capture_name.c_str();
    return "default";
}

int StyleMap::named_style_count() const {
    int n = 0;
    for (auto& s : styles_)
        if (!s.capture_name.empty()) ++n;
    return n;
}

} // namespace npp_ts
