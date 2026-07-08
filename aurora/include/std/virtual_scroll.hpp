#ifndef AURORA_VIRTUAL_SCROLL_HPP
#define AURORA_VIRTUAL_SCROLL_HPP

#include <cstdint>
#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AuroraVirtualScroll AuroraVirtualScroll;

AuroraVirtualScroll* aurora_virtual_scroll_create(int total_items, int item_height, int viewport_height);
void                 aurora_virtual_scroll_destroy(AuroraVirtualScroll* vs);
int                  aurora_virtual_scroll_get_first_visible(AuroraVirtualScroll* vs);
int                  aurora_virtual_scroll_get_last_visible(AuroraVirtualScroll* vs);
int                  aurora_virtual_scroll_get_total_height(AuroraVirtualScroll* vs);
void                 aurora_virtual_scroll_set_scroll_offset(AuroraVirtualScroll* vs, int offset);
int                  aurora_virtual_scroll_get_scroll_offset(AuroraVirtualScroll* vs);
int                  aurora_virtual_scroll_item_at_y(AuroraVirtualScroll* vs, int y);
void                 aurora_virtual_scroll_set_total_items(AuroraVirtualScroll* vs, int total);
int                  aurora_virtual_scroll_get_total_items(AuroraVirtualScroll* vs);

#ifdef __cplusplus
}
#endif

#endif
