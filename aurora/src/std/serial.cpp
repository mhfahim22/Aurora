#include "std/serial.hpp"
#include "std/json.hpp"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <vector>

/* ════════════════════════════════════════════════════════════
   JSON serialization (delegates to json.cpp)
   ════════════════════════════════════════════════════════════ */

char* aurora_serialize_json(void* json_val) {
    if (!json_val) return strdup("null");
    return aurora_json_serialize((JsonValue*)json_val);
}

void* aurora_deserialize_json(const char* json_str) {
    if (!json_str) return nullptr;
    return aurora_json_parse(json_str);
}

/* ════════════════════════════════════════════════════════════
   Binary serialization
   ════════════════════════════════════════════════════════════ */

static void bin_write_byte(std::vector<unsigned char>& buf, unsigned char b) {
    buf.push_back(b);
}

static void bin_write_bytes(std::vector<unsigned char>& buf, const void* data, int len) {
    const unsigned char* src = (const unsigned char*)data;
    for (int i = 0; i < len; i++) buf.push_back(src[i]);
}

static void bin_write_tag(std::vector<unsigned char>& buf, int tag) {
    bin_write_byte(buf, (unsigned char)(tag & 0xff));
}

static void bin_write_i32(std::vector<unsigned char>& buf, int val) {
    unsigned char bytes[4];
    bytes[0] = (unsigned char)(val & 0xff);
    bytes[1] = (unsigned char)((val >> 8) & 0xff);
    bytes[2] = (unsigned char)((val >> 16) & 0xff);
    bytes[3] = (unsigned char)((val >> 24) & 0xff);
    bin_write_bytes(buf, bytes, 4);
}

static void bin_write_double(std::vector<unsigned char>& buf, double val) {
    unsigned char bytes[8];
    unsigned long long* ptr = (unsigned long long*)&val;
    for (int i = 0; i < 8; i++) bytes[i] = (unsigned char)((*ptr >> (i * 8)) & 0xff);
    bin_write_bytes(buf, bytes, 8);
}

static void bin_write_string(std::vector<unsigned char>& buf, const char* str) {
    int len = str ? (int)strlen(str) : 0;
    bin_write_i32(buf, len);
    if (len > 0) bin_write_bytes(buf, str, len);
}

static void bin_serialize_value(JsonValue* val, std::vector<unsigned char>& buf);

static void bin_serialize_value(JsonValue* val, std::vector<unsigned char>& buf) {
    if (!val) {
        bin_write_tag(buf, BIN_NULL);
        return;
    }
    switch (val->type) {
        case JSON_NULL:
            bin_write_tag(buf, BIN_NULL);
            break;
        case JSON_BOOL:
            bin_write_tag(buf, BIN_BOOL);
            bin_write_byte(buf, (unsigned char)(val->num_val != 0 ? 1 : 0));
            break;
        case JSON_NUM: {
            double d = val->num_val;
            int64_t i = (int64_t)d;
            if ((double)i == d && i >= -2147483648LL && i <= 2147483647LL) {
                bin_write_tag(buf, BIN_INT32);
                bin_write_i32(buf, (int)i);
            } else {
                bin_write_tag(buf, BIN_DOUBLE);
                bin_write_double(buf, d);
            }
            break;
        }
        case JSON_STR:
            bin_write_tag(buf, BIN_STRING);
            bin_write_string(buf, val->str_val ? val->str_val : "");
            break;
        case JSON_ARRAY:
            bin_write_tag(buf, BIN_ARRAY);
            bin_write_i32(buf, val->count);
            for (int i = 0; i < val->count; i++)
                bin_serialize_value(val->items[i], buf);
            break;
        case JSON_OBJECT:
            bin_write_tag(buf, BIN_OBJECT);
            bin_write_i32(buf, val->count);
            for (int i = 0; i < val->count; i++) {
                bin_write_string(buf, val->keys[i]);
                bin_serialize_value(val->items[i], buf);
            }
            break;
        default:
            bin_write_tag(buf, BIN_NULL);
            break;
    }
}

unsigned char* aurora_serialize_binary(void* json_val, int* out_len) {
    if (!out_len) return nullptr;
    *out_len = 0;
    std::vector<unsigned char> buf;
    bin_serialize_value((JsonValue*)json_val, buf);
    if (buf.empty()) return nullptr;
    unsigned char* result = (unsigned char*)malloc(buf.size());
    if (!result) return nullptr;
    memcpy(result, buf.data(), buf.size());
    *out_len = (int)buf.size();
    return result;
}

/* ── Binary deserialization ── */

class BinReader {
public:
    BinReader(const unsigned char* data, int len) : data_(data), pos_(0), len_(len) {}

    unsigned char read_byte() {
        if (pos_ >= len_) return 0;
        return data_[pos_++];
    }

    int read_i32() {
        if (pos_ + 4 > len_) return 0;
        int val = (int)data_[pos_] |
                 ((int)data_[pos_+1] << 8) |
                 ((int)data_[pos_+2] << 16) |
                 ((int)data_[pos_+3] << 24);
        pos_ += 4;
        return val;
    }

    double read_double() {
        if (pos_ + 8 > len_) return 0;
        unsigned long long val = 0;
        for (int i = 0; i < 8; i++)
            val |= (unsigned long long)data_[pos_+i] << (i * 8);
        pos_ += 8;
        double result;
        memcpy(&result, &val, 8);
        return result;
    }

    char* read_string() {
        int len = read_i32();
        if (len < 0 || pos_ + len > len_) return strdup("");
        char* s = (char*)malloc((size_t)(len + 1));
        if (!s) return strdup("");
        memcpy(s, data_ + pos_, (size_t)len);
        s[len] = '\0';
        pos_ += len;
        return s;
    }

    int available() const { return len_ - pos_; }

private:
    const unsigned char* data_;
    int pos_;
    int len_;
};

static JsonValue* bin_deserialize_value(BinReader& reader);

static JsonValue* bin_deserialize_value(BinReader& reader) {
    if (reader.available() <= 0) return nullptr;
    int tag = reader.read_byte();
    switch (tag) {
        case BIN_NULL:
            return aurora_json_new();
        case BIN_BOOL: {
            JsonValue* v = aurora_json_new_num(0);
            v->type = JSON_BOOL;
            v->num_val = reader.read_byte() ? 1.0 : 0.0;
            return v;
        }
        case BIN_INT8:
        case BIN_INT16:
        case BIN_INT32: {
            int val = reader.read_i32();
            JsonValue* v = aurora_json_new_num((double)val);
            return v;
        }
        case BIN_INT64: {
            /* Read low 32 bits as approximation */
            int lo = reader.read_i32();
            reader.read_i32(); /* discard high */
            return aurora_json_new_num((double)lo);
        }
        case BIN_FLOAT: {
            double d = reader.read_double();
            return aurora_json_new_num(d);
        }
        case BIN_DOUBLE: {
            double d = reader.read_double();
            return aurora_json_new_num(d);
        }
        case BIN_STRING: {
            char* s = reader.read_string();
            JsonValue* v = aurora_json_new_str(s);
            free(s);
            return v;
        }
        case BIN_ARRAY: {
            int count = reader.read_i32();
            JsonValue* arr = aurora_json_new_array();
            for (int i = 0; i < count; i++) {
                JsonValue* item = bin_deserialize_value(reader);
                if (item) aurora_json_array_push(arr, item);
            }
            return arr;
        }
        case BIN_OBJECT: {
            int count = reader.read_i32();
            JsonValue* obj = aurora_json_new_object();
            for (int i = 0; i < count; i++) {
                char* key = reader.read_string();
                JsonValue* val = bin_deserialize_value(reader);
                if (key && val) {
                    aurora_json_set_obj(obj, key, val);
                }
                free(key);
            }
            return obj;
        }
        default:
            return aurora_json_new();
    }
}

void* aurora_deserialize_binary(const unsigned char* data, int data_len) {
    if (!data || data_len <= 0) return nullptr;
    BinReader reader(data, data_len);
    return bin_deserialize_value(reader);
}

/* ════════════════════════════════════════════════════════════
   Unified serializer
   ════════════════════════════════════════════════════════════ */

char* aurora_serialize(void* json_val, int format) {
    if (format == SERIAL_FORMAT_JSON) {
        return aurora_serialize_json(json_val);
    }
    /* For binary, return hex-encoded string representation */
    if (format == SERIAL_FORMAT_BINARY) {
        int len = 0;
        unsigned char* bin = aurora_serialize_binary(json_val, &len);
        if (!bin || len <= 0) {
            free(bin);
            return strdup("");
        }
        char* hex = (char*)malloc((size_t)(len * 2 + 1));
        if (!hex) { free(bin); return strdup(""); }
        for (int i = 0; i < len; i++) {
            sprintf(hex + i * 2, "%02x", bin[i]);
        }
        hex[len * 2] = '\0';
        free(bin);
        return hex;
    }
    return strdup("");
}

void* aurora_deserialize(const char* data, int format) {
    if (!data) return nullptr;
    if (format == SERIAL_FORMAT_JSON) {
        return aurora_deserialize_json(data);
    }
    if (format == SERIAL_FORMAT_BINARY) {
        /* Decode hex string to binary */
        int hex_len = (int)strlen(data);
        if (hex_len % 2 != 0) return nullptr;
        int bin_len = hex_len / 2;
        unsigned char* bin = (unsigned char*)malloc((size_t)bin_len);
        if (!bin) return nullptr;
        for (int i = 0; i < bin_len; i++) {
            unsigned int byte;
            sscanf(data + i * 2, "%2x", &byte);
            bin[i] = (unsigned char)byte;
        }
        void* result = aurora_deserialize_binary(bin, bin_len);
        free(bin);
        return result;
    }
    return nullptr;
}

/* ════════════════════════════════════════════════════════════
   File I/O
   ════════════════════════════════════════════════════════════ */

int aurora_serialize_to_file(void* json_val, const char* filepath, int format) {
    if (!json_val || !filepath) return -1;
    if (format == SERIAL_FORMAT_BINARY) {
        int len = 0;
        unsigned char* bin = aurora_serialize_binary(json_val, &len);
        if (!bin) return -1;
        FILE* f = fopen(filepath, "wb");
        if (!f) { free(bin); return -1; }
        fwrite(bin, 1, (size_t)len, f);
        fclose(f);
        free(bin);
        return len;
    }
    /* Default: JSON */
    char* json = aurora_serialize_json(json_val);
    if (!json) return -1;
    FILE* f = fopen(filepath, "w");
    if (!f) { free(json); return -1; }
    int len = (int)strlen(json);
    fwrite(json, 1, (size_t)len, f);
    fclose(f);
    free(json);
    return len;
}

void* aurora_deserialize_from_file(const char* filepath, int format) {
    if (!filepath) return nullptr;
    int detected = aurora_serial_detect_format(filepath);
    if (detected >= 0) format = detected;

    if (format == SERIAL_FORMAT_BINARY) {
        FILE* f = fopen(filepath, "rb");
        if (!f) return nullptr;
        fseek(f, 0, SEEK_END);
        long len = ftell(f);
        fseek(f, 0, SEEK_SET);
        if (len <= 0) { fclose(f); return nullptr; }
        unsigned char* buf = (unsigned char*)malloc((size_t)len);
        if (!buf) { fclose(f); return nullptr; }
        fread(buf, 1, (size_t)len, f);
        fclose(f);
        void* result = aurora_deserialize_binary(buf, (int)len);
        free(buf);
        return result;
    }

    /* JSON */
    FILE* f = fopen(filepath, "r");
    if (!f) return nullptr;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len <= 0) { fclose(f); return nullptr; }
    char* buf = (char*)malloc((size_t)(len + 1));
    if (!buf) { fclose(f); return nullptr; }
    fread(buf, 1, (size_t)len, f);
    buf[len] = '\0';
    fclose(f);
    void* result = aurora_deserialize_json(buf);
    free(buf);
    return result;
}

/* ════════════════════════════════════════════════════════════
   Format detection
   ════════════════════════════════════════════════════════════ */

int aurora_serial_detect_format(const char* filename) {
    if (!filename) return SERIAL_FORMAT_JSON;
    const char* dot = strrchr(filename, '.');
    if (!dot) return SERIAL_FORMAT_JSON;
    dot++; /* skip '.' */
#ifdef _WIN32
    if (_stricmp(dot, "json") == 0) return SERIAL_FORMAT_JSON;
    if (_stricmp(dot, "bin") == 0)  return SERIAL_FORMAT_BINARY;
    if (_stricmp(dot, "dat") == 0)  return SERIAL_FORMAT_BINARY;
#else
    if (strcasecmp(dot, "json") == 0) return SERIAL_FORMAT_JSON;
    if (strcasecmp(dot, "bin") == 0)  return SERIAL_FORMAT_BINARY;
    if (strcasecmp(dot, "dat") == 0)  return SERIAL_FORMAT_BINARY;
#endif
    return SERIAL_FORMAT_JSON;
}
