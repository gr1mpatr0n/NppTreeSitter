// src/style_map.h
// Maps tree-sitter highlight capture names (e.g. "keyword", "function")
// to Scintilla style numbers (0–31 usable range for custom lexers).
#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <cstdint>

namespace npp_ts {

/// A single style entry: a Scintilla style number plus a human‐readable name.
struct StyleEntry {
    uint8_t     scintilla_style;   // 0–31 for custom lexer styles
    std::string capture_name;      // e.g. "keyword", "function.builtin"
};

/// StyleMap holds the bidirectional mapping between tree-sitter capture names
/// and Scintilla style IDs.  It is populated from the config XML file.
class StyleMap {
public:
    StyleMap();

    /// Load mappings from a simple XML/INI config file.
    /// Returns true on success.
    bool load_from_file(const std::string& path);

    /// Look up the Scintilla style for a given capture name.
    /// Falls back to dotted-prefix matching: "function.builtin" → "function" → default.
    uint8_t style_for_capture(std::string_view capture_name) const;

    /// Get a human-readable name for a style number (for the style configurator).
    const char* name_of_style(uint8_t style) const;

    /// How many named styles we have.
    int named_style_count() const;

    /// Build the default mapping (used if no config file is found).
    void load_defaults();

private:
    std::unordered_map<std::string, uint8_t> capture_to_style_;
    std::vector<StyleEntry>                  styles_;     // indexed by style number
    uint8_t                                  default_style_ = 0;
};

} // namespace npp_ts
