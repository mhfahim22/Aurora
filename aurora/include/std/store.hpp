#pragma once
#include <cstdint>
#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

/* ── In-App Purchases & Subscriptions (8) ── */
void*       aurora_store_init(void);
int         aurora_store_products(void* handle, const char* product_ids_json);
int         aurora_store_purchase(void* handle, const char* product_id);
int         aurora_store_restore_purchases(void* handle);
int         aurora_store_is_purchased(void* handle, const char* product_id);
const char* aurora_store_get_receipt(void* handle);
int         aurora_store_consume(void* handle, const char* product_id);
void        aurora_store_shutdown(void* handle);

#ifdef __cplusplus
}
#endif
