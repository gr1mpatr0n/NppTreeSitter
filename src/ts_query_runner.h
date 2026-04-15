// src/ts_query_runner.h
// Runs a tree-sitter highlight query against a syntax tree and produces
// an array of (position, length, style) triples to apply to Scintilla.
#pragma once

#include <vector>
#include <cstdint>

#include <tree_sitter/api.h>
#include "style_map.h"

namespace npp_ts {

/// A single styled range: [start, start+length) should be painted with `style`.
struct StyledRange {
    uint32_t start;
    uint32_t length;
    uint8_t  style;
};

/// Run `query` over the region [start_byte, start_byte + length) of `tree`,
/// mapping capture names to styles via `style_map`.
///
/// The returned vector is sorted by start position with no overlaps; later
/// (more-specific) captures override earlier ones.
std::vector<StyledRange> run_highlight_query(
    const TSTree*    tree,
    const TSQuery*   query,
    const StyleMap&  style_map,
    uint32_t         start_byte,
    uint32_t         length
);

} // namespace npp_ts
