#pragma once
#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Format constants ── */
#define SERIAL_FORMAT_JSON   0
#define SERIAL_FORMAT_BINARY 1
#define SERIAL_FORMAT_AUTODETECT -1

/* ── Binary format type tags ── */
#define BIN_NULL    0
#define BIN_BOOL    1
#define BIN_INT8    2
#define BIN_INT16   3
#define BIN_INT32   4
#define BIN_INT64   5
#define BIN_FLOAT   6
#define BIN_DOUBLE  7
#define BIN_STRING  8
#define BIN_ARRAY   9
#define BIN_OBJECT  10

/* ── Serialization API ── */

/* Serialize a JSON value to a string (wrapper around aurora_json_serialize) */
char* aurora_serialize_json(void* json_val);

/* Parse a JSON string to a JSON value (wrapper around aurora_json_parse) */
void* aurora_deserialize_json(const char* json_str);

/* ── Binary serialization ── */

/* Serialize a JsonValue to binary format. Returns malloc'd buffer, sets out_len. */
unsigned char* aurora_serialize_binary(void* json_val, int* out_len);

/* Deserialize binary data back to a JsonValue. Returns NULL on failure. */
void* aurora_deserialize_binary(const unsigned char* data, int data_len);

/* ── Unified serializer — auto-detect format from extension/prefix ── */

/* Serialize to string (JSON only for string output). Returns malloc'd string. */
char* aurora_serialize(void* json_val, int format);

/* Deserialize from string (JSON only). Returns JsonValue*. */
void* aurora_deserialize(const char* data, int format);

/* ── File I/O ── */

/* Write serialized data to file. Returns bytes written or -1. */
int aurora_serialize_to_file(void* json_val, const char* filepath, int format);

/* Read and deserialize from file. Returns JsonValue* or NULL. */
void* aurora_deserialize_from_file(const char* filepath, int format);

/* ── Utility ── */

/* Detect format from filename extension. Returns SERIAL_FORMAT_* */
int aurora_serial_detect_format(const char* filename);

#ifdef __cplusplus
}
#endif
