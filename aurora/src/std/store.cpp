#include "std/store.hpp"
#include "std/json.hpp"
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <string>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define PATH_SEP '\\'
#else
#include <unistd.h>
#include <sys/stat.h>
#define PATH_SEP '/'
#endif

struct StoreHandle {
    char product_cache[8192];
    int initialized;
};

void* aurora_store_init(void) {
    StoreHandle* h = (StoreHandle*)calloc(1, sizeof(StoreHandle));
    if (h) h->initialized = 1;
    return h;
}

int aurora_store_products(void* handle, const char* product_ids_json) {
    if (!handle || !product_ids_json) return 0;
    StoreHandle* h = (StoreHandle*)handle;

    /* Simulate store product response */
    JsonValue* root = aurora_json_new_object();
    JsonValue* products = aurora_json_new_array();

    JsonValue* ids = aurora_json_parse(product_ids_json);
    if (ids && aurora_json_type(ids) == JSON_ARRAY) {
        for (int i = 0; i < ids->count; i++) {
            JsonValue* idv = aurora_json_array_get(ids, i);
            if (idv && aurora_json_type(idv) == JSON_STR && idv->str_val) {
                JsonValue* prod = aurora_json_new_object();
                aurora_json_set_str(prod, "product_id", idv->str_val);
                aurora_json_set_str(prod, "title", idv->str_val);
                aurora_json_set_str(prod, "price", "$9.99");
                aurora_json_set_str(prod, "currency", "USD");
                aurora_json_array_push(products, prod);
            }
        }
    }
    aurora_json_free(ids);

    aurora_json_set_obj(root, "products", products);
    char* json = aurora_json_serialize(root);
    if (json) {
        strncpy(h->product_cache, json, sizeof(h->product_cache) - 1);
        free(json);
    }
    aurora_json_free(root);
    return 1;
}

int aurora_store_purchase(void* handle, const char* product_id) {
    if (!handle || !product_id) return 0;
    /* Simulate successful purchase */
    return 1;
}

int aurora_store_restore_purchases(void* handle) {
    if (!handle) return 0;
    /* Simulate restoring purchased products */
    return 1;
}

int aurora_store_is_purchased(void* handle, const char* product_id) {
    if (!handle || !product_id) return 0;
    /* Simulate: check if product is purchased */
    (void)product_id;
    return 1;
}

const char* aurora_store_get_receipt(void* handle) {
    if (!handle) return nullptr;
    (void)handle;
    return "{\"receipt\":\"simulated_receipt_data\"}";
}

int aurora_store_consume(void* handle, const char* product_id) {
    if (!handle || !product_id) return 0;
    /* Consumable product consumed */
    return 1;
}

void aurora_store_shutdown(void* handle) {
    if (handle) free(handle);
}
