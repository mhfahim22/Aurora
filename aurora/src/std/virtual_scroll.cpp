#include "std/virtual_scroll.hpp"
#include <cstdlib>
#include <algorithm>

struct AuroraVirtualScroll {
    int total_items;
    int item_height;
    int viewport_height;
    int scroll_offset;
};

AuroraVirtualScroll* aurora_virtual_scroll_create(int total_items, int item_height, int viewport_height) {
    if (item_height <= 0 || viewport_height <= 0) return nullptr;
    AuroraVirtualScroll* vs = (AuroraVirtualScroll*)malloc(sizeof(AuroraVirtualScroll));
    if (vs) {
        vs->total_items = (total_items > 0) ? total_items : 0;
        vs->item_height = item_height;
        vs->viewport_height = viewport_height;
        vs->scroll_offset = 0;
    }
    return vs;
}

void aurora_virtual_scroll_destroy(AuroraVirtualScroll* vs) {
    if (vs) free(vs);
}

int aurora_virtual_scroll_get_first_visible(AuroraVirtualScroll* vs) {
    if (!vs || vs->total_items == 0) return 0;
    return std::max(0, vs->scroll_offset / vs->item_height);
}

int aurora_virtual_scroll_get_last_visible(AuroraVirtualScroll* vs) {
    if (!vs || vs->total_items == 0) return 0;
    int last = (vs->scroll_offset + vs->viewport_height) / vs->item_height;
    return std::min(vs->total_items - 1, last);
}

int aurora_virtual_scroll_get_total_height(AuroraVirtualScroll* vs) {
    if (!vs) return 0;
    return vs->total_items * vs->item_height;
}

void aurora_virtual_scroll_set_scroll_offset(AuroraVirtualScroll* vs, int offset) {
    if (!vs) return;
    int max_offset = std::max(0, vs->total_items * vs->item_height - vs->viewport_height);
    vs->scroll_offset = std::max(0, std::min(offset, max_offset));
}

int aurora_virtual_scroll_get_scroll_offset(AuroraVirtualScroll* vs) {
    return vs ? vs->scroll_offset : 0;
}

int aurora_virtual_scroll_item_at_y(AuroraVirtualScroll* vs, int y) {
    if (!vs || vs->item_height == 0) return -1;
    int idx = (y + vs->scroll_offset) / vs->item_height;
    return (idx >= 0 && idx < vs->total_items) ? idx : -1;
}

void aurora_virtual_scroll_set_total_items(AuroraVirtualScroll* vs, int total) {
    if (!vs) return;
    vs->total_items = (total > 0) ? total : 0;
    if (vs->scroll_offset > 0)
        aurora_virtual_scroll_set_scroll_offset(vs, vs->scroll_offset);
}

int aurora_virtual_scroll_get_total_items(AuroraVirtualScroll* vs) {
    return vs ? vs->total_items : 0;
}
