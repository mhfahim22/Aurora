#include "std/i18n.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#else
#include <locale.h>
#endif

static std::mutex g_i18n_mtx;

struct TranslationEntry {
    std::string lang;
    std::string key;
    std::string value;
};

struct TranslationDomain {
    std::string name;
    std::vector<TranslationEntry> entries;
    std::vector<std::string> available_langs;
};

static std::vector<TranslationDomain> g_domains;
static std::string g_current_locale = "en";

static TranslationDomain* find_domain(const std::string& name) {
    for (auto& d : g_domains) if (d.name == name) return &d;
    return nullptr;
}

static std::string tr(const std::string& domain, const std::string& lang, const std::string& key) {
    for (auto& d : g_domains) {
        if (d.name == domain) {
            for (auto& e : d.entries) {
                if (e.lang == lang && e.key == key) return e.value;
            }
            break;
        }
    }
    for (auto& d : g_domains) {
        if (d.name == domain) {
            std::string fallback = lang.substr(0, 2);
            for (auto& e : d.entries) {
                std::string el = e.lang.substr(0, 2);
                if (el == fallback && e.key == key) return e.value;
            }
            for (auto& e : d.entries) {
                if (e.lang == "en" && e.key == key) return e.value;
            }
            break;
        }
    }
    return key;
}

int aurora_i18n_locale(AuroraLocale* out) {
    if (!out) return 0;
    memset(out, 0, sizeof(AuroraLocale));
    std::string loc = g_current_locale;
    size_t dash = loc.find('-');
    if (dash != std::string::npos) {
        strncpy(out->language, loc.substr(0, dash).c_str(), sizeof(out->language) - 1);
        strncpy(out->region, loc.substr(dash + 1).c_str(), sizeof(out->region) - 1);
    } else {
        strncpy(out->language, loc.c_str(), sizeof(out->language) - 1);
    }
    return 1;
}

int aurora_i18n_set_locale(const char* lang) {
    if (!lang) return 0;
    std::lock_guard<std::mutex> lock(g_i18n_mtx);
    g_current_locale = lang;
    return 1;
}

const char* aurora_i18n_get_locale(void) {
    static std::string result;
    result = g_current_locale;
    return result.c_str();
}

const char* aurora_i18n_get_system_locale(void) {
    static std::string result;
#ifdef _WIN32
    char buf[16];
    int n = GetLocaleInfoA(LOCALE_USER_DEFAULT, LOCALE_SISO639LANGNAME, buf, sizeof(buf));
    if (n > 0) {
        result = buf;
        char region[16];
        if (GetLocaleInfoA(LOCALE_USER_DEFAULT, LOCALE_SISO3166CTRYNAME, region, sizeof(region)) > 0) {
            result += "-";
            result += region;
        }
    } else result = "en";
#else
    const char* l = setlocale(LC_ALL, "");
    if (l) result = l;
    else result = "en";
#endif
    return result.c_str();
}

const char* aurora_i18n_translate(const char* key, const char* domain) {
    if (!key) return nullptr;
    std::lock_guard<std::mutex> lock(g_i18n_mtx);
    std::string dom = domain ? domain : "default";
    static std::string result;
    result = tr(dom, g_current_locale, key);
    return result.c_str();
}

int aurora_i18n_load(const char* domain, const char* filepath) {
    if (!domain || !filepath) return 0;
    FILE* f = fopen(filepath, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::string data((size_t)len, '\0');
    fread(&data[0], 1, (size_t)len, f);
    fclose(f);
    return aurora_i18n_load_json(domain, data.c_str());
}

static size_t json_skip_string(const std::string& js, size_t pos) {
    if (pos >= js.size() || js[pos] != '"') return pos;
    pos++;
    while (pos < js.size()) {
        if (js[pos] == '\\' && pos + 1 < js.size()) { pos += 2; continue; }
        if (js[pos] == '"') return pos + 1;
        pos++;
    }
    return std::string::npos;
}

static size_t json_skip_value(const std::string& js, size_t pos) {
    if (pos >= js.size()) return pos;
    while (pos < js.size() && (js[pos] == ' ' || js[pos] == '\t' || js[pos] == '\n' || js[pos] == '\r')) pos++;
    if (pos >= js.size()) return pos;
    if (js[pos] == '"') return json_skip_string(js, pos);
    if (js[pos] == '{' || js[pos] == '[') {
        char open = js[pos];
        char close = (open == '{') ? '}' : ']';
        pos++;
        int depth = 1;
        while (pos < js.size() && depth > 0) {
            if (js[pos] == '"') pos = json_skip_string(js, pos);
            else if (js[pos] == open) depth++;
            else if (js[pos] == close) depth--;
            else pos++;
        }
        return pos + 1;
    }
    while (pos < js.size() && js[pos] != ',' && js[pos] != '}' && js[pos] != ']' &&
           js[pos] != ' ' && js[pos] != '\t' && js[pos] != '\n' && js[pos] != '\r')
        pos++;
    return pos;
}

int aurora_i18n_load_json(const char* domain, const char* json_data) {
    if (!domain || !json_data) return 0;
    std::lock_guard<std::mutex> lock(g_i18n_mtx);
    std::string dom = domain;
    TranslationDomain* d = find_domain(dom);
    if (!d) {
        g_domains.push_back({dom, {}, {}});
        d = &g_domains.back();
    }
    std::string js(json_data);
    size_t pos = 0;
    while ((pos = js.find('"', pos)) != std::string::npos) {
        size_t kstart = pos + 1;
        if (kstart >= js.size()) break;
        size_t kend = js.find('"', kstart);
        if (kend == std::string::npos) break;
        if (kend > 0 && js[kend - 1] == '\\') {
            pos = kend + 1;
            continue;
        }
        std::string key = js.substr(kstart, kend - kstart);
        if (key.empty() || key[0] == '@' || key[0] == '$') {
            pos = kend + 1;
            continue;
        }
        pos = kend + 1;
        while (pos < js.size() && (js[pos] == ' ' || js[pos] == '\t' || js[pos] == '\n' || js[pos] == '\r')) pos++;
        if (pos >= js.size() || js[pos] != ':') continue;
        pos++;
        while (pos < js.size() && (js[pos] == ' ' || js[pos] == '\t' || js[pos] == '\n' || js[pos] == '\r')) pos++;
        if (pos >= js.size()) break;
        if (js[pos] == '{') {
            size_t lang_end = json_skip_value(js, pos);
            if (lang_end != std::string::npos) pos = lang_end;
            continue;
        }
        if (js[pos] == '"') {
            size_t vstart = pos + 1;
            size_t vend = pos;
            while (vend < js.size()) {
                if (js[vend] == '\\' && vend + 1 < js.size()) { vend += 2; continue; }
                if (js[vend] == '"') break;
                vend++;
            }
            if (vend >= js.size()) break;
            std::string value = js.substr(vstart, vend - vstart);
            d->entries.push_back({g_current_locale, key, value});
            pos = vend + 1;
        }
    }
    auto& langs = d->available_langs;
    if (std::find(langs.begin(), langs.end(), g_current_locale) == langs.end())
        langs.push_back(g_current_locale);
    return 1;
}

int aurora_i18n_add(const char* domain, const char* lang, const char* key, const char* value) {
    if (!domain || !lang || !key || !value) return 0;
    std::lock_guard<std::mutex> lock(g_i18n_mtx);
    std::string dom = domain;
    TranslationDomain* d = find_domain(dom);
    if (!d) {
        g_domains.push_back({dom, {}, {}});
        d = &g_domains.back();
    }
    d->entries.push_back({lang, key, value});
    if (std::find(d->available_langs.begin(), d->available_langs.end(), lang) == d->available_langs.end())
        d->available_langs.push_back(lang);
    return 1;
}

const char* aurora_i18n_format(const char* pattern, int argc, const char** args) {
    if (!pattern) return nullptr;
    std::lock_guard<std::mutex> lock(g_i18n_mtx);
    static std::string result;
    result.clear();
    std::string p(pattern);
    for (size_t i = 0; i < p.size(); i++) {
        if (p[i] == '{') {
            size_t j = i + 1;
            while (j < p.size() && p[j] != '}') j++;
            if (j < p.size()) {
                std::string idx_str = p.substr(i + 1, j - i - 1);
                int idx = atoi(idx_str.c_str());
                if (idx >= 0 && idx < argc && args[idx]) result += args[idx];
                i = j;
            }
        } else {
            result += p[i];
        }
    }
    return result.c_str();
}

int aurora_i18n_rtl(const char* locale) {
    if (!locale) return 0;
    static const char* rtl_locales[] = {"ar", "he", "fa", "ur", "yi", NULL};
    for (int i = 0; rtl_locales[i]; i++) {
        if (strncmp(locale, rtl_locales[i], 2) == 0) return 1;
    }
    return 0;
}

int aurora_i18n_language_name(const char* lang, char* buf, int bufsize) {
    if (!lang || !buf || bufsize <= 0) return 0;
    static const struct { const char* code; const char* name; } names[] = {
        {"en", "English"}, {"bn", "Bengali"}, {"es", "Spanish"},
        {"fr", "French"}, {"de", "German"}, {"zh", "Chinese"},
        {"ja", "Japanese"}, {"ko", "Korean"}, {"ar", "Arabic"},
        {"hi", "Hindi"}, {"pt", "Portuguese"}, {"ru", "Russian"},
        {NULL, NULL}
    };
    for (int i = 0; names[i].code; i++) {
        if (strcmp(lang, names[i].code) == 0) {
            strncpy(buf, names[i].name, (size_t)bufsize - 1);
            buf[bufsize - 1] = '\0';
            return 1;
        }
    }
    strncpy(buf, lang, (size_t)bufsize - 1);
    buf[bufsize - 1] = '\0';
    return 1;
}

int aurora_i18n_available_locales(char* buf, int bufsize) {
    if (!buf || bufsize <= 0) return 0;
    std::lock_guard<std::mutex> lock(g_i18n_mtx);
    std::string result;
    for (auto& d : g_domains) {
        for (auto& l : d.available_langs) {
            if (!result.empty()) result += ",";
            result += l;
        }
    }
    if (result.empty()) result = "en";
    strncpy(buf, result.c_str(), (size_t)bufsize - 1);
    buf[bufsize - 1] = '\0';
    return 1;
}
