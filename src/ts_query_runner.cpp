// src/ts_query_runner.cpp
#include "ts_query_runner.h"

#include <algorithm>
#include <cstring>

namespace npp_ts {

std::vector<StyledRange> run_highlight_query(
    const TSTree*    tree,
    const TSQuery*   query,
    const StyleMap&  style_map,
    uint32_t         start_byte,
    uint32_t         end_byte)
{
    if (!tree || !query) return {};

    TSNode root = ts_tree_root_node(tree);

    // Create a query cursor restricted to the byte range we need to style.
    TSQueryCursor* cursor = ts_query_cursor_new();
    ts_query_cursor_set_byte_range(cursor, start_byte, end_byte);
    ts_query_cursor_exec(cursor, query, root);

    // We collect all matches, converting captures to styled ranges.
    // Later captures override earlier ones (tree-sitter gives more specific
    // matches later), so we build an overlay.
    //
    // Strategy: collect all (start, end, style) triples, then merge.

    struct RawRange {
        uint32_t start;
        uint32_t end;
        uint8_t  style;
        uint32_t pattern_index;  // higher = more specific
    };

    std::vector<RawRange> raw;

    TSQueryMatch match;
    while (ts_query_cursor_next_match(cursor, &match)) {
        for (uint16_t ci = 0; ci < match.capture_count; ++ci) {
            TSQueryCapture cap = match.captures[ci];
            uint32_t cap_name_len = 0;
            const char* cap_name = ts_query_capture_name_for_id(
                query, cap.index, &cap_name_len);

            uint8_t style = style_map.style_for_capture(
                std::string_view(cap_name, cap_name_len));

            uint32_t s = ts_node_start_byte(cap.node);
            uint32_t e = ts_node_end_byte(cap.node);

            // Clamp to our range.
            if (s < start_byte) s = start_byte;
            if (e > end_byte)   e = end_byte;
            if (s >= e) continue;

            raw.push_back({s, e, style, match.pattern_index});
        }
    }

    ts_query_cursor_delete(cursor);

    if (raw.empty()) return {};

    // Sort by start, then by pattern_index ascending (so later patterns win).
    std::sort(raw.begin(), raw.end(), [](const RawRange& a, const RawRange& b) {
        if (a.start != b.start) return a.start < b.start;
        return a.pattern_index < b.pattern_index;
    });

    // Paint onto a style buffer.
    uint32_t total = end_byte - start_byte;
    std::vector<uint8_t> style_buf(total, 0);  // 0 = default

    for (auto& r : raw) {
        uint32_t s = r.start - start_byte;
        uint32_t e = r.end   - start_byte;
        std::memset(&style_buf[s], r.style, e - s);
    }

    // Run-length encode into StyledRange.
    std::vector<StyledRange> result;
    uint32_t run_start = 0;
    uint8_t  run_style = style_buf[0];

    for (uint32_t i = 1; i <= total; ++i) {
        uint8_t cur = (i < total) ? style_buf[i] : 0xFF; // sentinel
        if (cur != run_style) {
            result.push_back({
                start_byte + run_start,
                i - run_start,
                run_style
            });
            run_start = i;
            run_style = cur;
        }
    }

    return result;
}

} // namespace npp_ts
