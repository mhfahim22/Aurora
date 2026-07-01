#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <fstream>
#include <filesystem>
#include <cstdint>
#include <optional>
#include <sstream>
#include <iostream>
#include <cstring>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace fs = std::filesystem;

struct CacheEntry {
    std::string hash;
    int64_t file_size;
    fs::file_time_type::clock::rep mtime;
};

class BuildCache {
public:
    explicit BuildCache(const std::string& cache_dir = ".aurora_cache")
        : cache_dir_(cache_dir) {
        fs::create_directories(cache_dir_);
        load();
    }

    bool is_up_to_date(const std::vector<std::string>& deps,
                       const std::string& output_path,
                       const std::string& flags_hash = "") {
        if (output_path.empty()) return false;
        if (!fs::exists(output_path)) return false;

        auto it = cache_.find(output_path);
        if (it == cache_.end()) return false;

        auto& cached = it->second;

        if (cached.flags_hash != flags_hash) return false;
        if (!cached.compiler_hash.empty() && cached.compiler_hash != compute_compiler_hash())
            return false;
        if (cached.deps.size() != deps.size()) return false;

        for (size_t i = 0; i < deps.size(); i++) {
            auto current = compute_entry(deps[i]);
            if (!current.has_value()) return false;
            if (current->hash != cached.deps[i].hash ||
                current->file_size != cached.deps[i].file_size ||
                current->mtime != cached.deps[i].mtime) {
                return false;
            }
        }
        return true;
    }

    void mark_cached(const std::string& output_path,
                     const std::vector<std::string>& deps,
                     const std::string& flags_hash = "") {
        if (output_path.empty()) return;
        auto& cached = cache_[output_path];
        cached.deps.clear();
        cached.flags_hash = flags_hash;
        cached.compiler_hash = compute_compiler_hash();
        for (auto& dep : deps) {
            auto entry = compute_entry(dep);
            if (entry.has_value()) {
                cached.deps.push_back(*entry);
            }
        }
        save();
    }

    static std::string compute_hash(const std::string& path) {
        std::ifstream f(path, std::ios::binary | std::ios::ate);
        if (!f) return sha256_empty();
        auto size = f.tellg();
        if (size <= 0) return sha256_empty();
        f.seekg(0);
        std::string content(static_cast<size_t>(size), '\0');
        f.read(content.data(), size);
        return sha256_hex(content);
    }

    static std::string sha256_hex(const std::string& data) {
        return sha256(data);
    }

private:
    struct CachedOutput {
        std::vector<CacheEntry> deps;
        std::string flags_hash;
        std::string compiler_hash;
    };

    std::string cache_dir_;
    std::unordered_map<std::string, CachedOutput> cache_;

    static std::string sha256_empty() {
        return "sha256-e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";
    }

    static std::string sha256(const std::string& data) {
        uint32_t h[8] = {
            0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
            0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
        };
        const uint32_t k[64] = {
            0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
            0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
            0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
            0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
            0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
            0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
            0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
            0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
            0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
            0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
            0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
            0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
            0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
            0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
            0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
            0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
        };
        auto rotr = [](uint32_t x, uint32_t n) -> uint32_t {
            return (x >> n) | (x << (32 - n));
        };
        uint64_t bit_len = data.size() * 8;
        size_t new_len = ((data.size() + 8 + 64) / 64) * 64;
        std::vector<uint8_t> buf(new_len, 0);
        memcpy(buf.data(), data.data(), data.size());
        buf[data.size()] = 0x80;
        for (size_t i = 0; i < 8; i++)
            buf[new_len - 8 + i] = (uint8_t)(bit_len >> (56 - i * 8));
        for (size_t block = 0; block < new_len; block += 64) {
            uint32_t w[64];
            for (int t = 0; t < 16; t++)
                w[t] = ((uint32_t)buf[block + t * 4] << 24)
                     | ((uint32_t)buf[block + t * 4 + 1] << 16)
                     | ((uint32_t)buf[block + t * 4 + 2] << 8)
                     | ((uint32_t)buf[block + t * 4 + 3]);
            for (int t = 16; t < 64; t++) {
                uint32_t s0 = rotr(w[t - 15], 7) ^ rotr(w[t - 15], 18) ^ (w[t - 15] >> 3);
                uint32_t s1 = rotr(w[t - 2], 17) ^ rotr(w[t - 2], 19) ^ (w[t - 2] >> 10);
                w[t] = w[t - 16] + s0 + w[t - 7] + s1;
            }
            uint32_t a = h[0], b = h[1], c = h[2], d = h[3];
            uint32_t e = h[4], f = h[5], g = h[6], hh = h[7];
            for (int t = 0; t < 64; t++) {
                uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
                uint32_t ch = (e & f) ^ ((~e) & g);
                uint32_t temp1 = hh + S1 + ch + k[t] + w[t];
                uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
                uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
                uint32_t temp2 = S0 + maj;
                hh = g; g = f; f = e; e = d + temp1;
                d = c; c = b; b = a; a = temp1 + temp2;
            }
            h[0] += a; h[1] += b; h[2] += c; h[3] += d;
            h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
        }
        std::string hex;
        const char* hex_chars = "0123456789abcdef";
        for (int i = 0; i < 8; i++)
            for (int j = 0; j < 4; j++) {
                uint8_t byte = (uint8_t)(h[i] >> (24 - j * 8));
                hex += hex_chars[(byte >> 4) & 0xf];
                hex += hex_chars[byte & 0xf];
            }
        return "sha256-" + hex;
    }

    static std::string compute_compiler_hash() {
        std::error_code ec;
        fs::file_time_type mod_time;
        std::string exe_name = "aurorac";
#ifdef _WIN32
        exe_name += ".exe";
#endif
        /* Try the actual running executable's path first */
        {
            char self_buf[1024] = {};
#ifdef _WIN32
            GetModuleFileNameA(nullptr, self_buf, sizeof(self_buf));
#else
            ssize_t len = readlink("/proc/self/exe", self_buf, sizeof(self_buf) - 1);
            if (len > 0) self_buf[len] = '\0';
#endif
            if (self_buf[0]) {
                mod_time = fs::last_write_time(self_buf, ec);
                if (!ec) goto done;
            }
        }
        /* Fallback: search common locations */
        mod_time = fs::last_write_time(fs::path(".") / exe_name, ec);
        if (ec) {
            mod_time = fs::last_write_time(fs::path(".") / "build/Release" / exe_name, ec);
            if (ec) {
                mod_time = fs::last_write_time(fs::path("..") / "build/Release" / exe_name, ec);
                if (ec) return "";
            }
        }
done:
        std::stringstream ss;
        ss << exe_name << "," << mod_time.time_since_epoch().count();
        return sha256(ss.str());
    }

    std::optional<CacheEntry> compute_entry(const std::string& path) {
        std::error_code ec;
        auto mtime = fs::last_write_time(path, ec);
        if (ec) return std::nullopt;
        auto size = fs::file_size(path, ec);
        if (ec) return std::nullopt;
        return CacheEntry{compute_hash(path),
                         static_cast<int64_t>(size),
                         mtime.time_since_epoch().count()};
    }

    void load() {
        auto cache_path = cache_dir_ + "/cache.txt";
        std::ifstream f(cache_path);
        if (!f) return;
        std::string line;
        while (std::getline(f, line)) {
            if (line.empty()) continue;
            auto eq = line.find('=');
            if (eq == std::string::npos) continue;
            std::string output = line.substr(0, eq);
            std::string rest = line.substr(eq + 1);

            CachedOutput co;

            size_t colon = rest.find(':');
            if (colon != std::string::npos) {
                co.flags_hash = rest.substr(0, colon);
                rest = rest.substr(colon + 1);
            }

            colon = rest.find(':');
            if (colon != std::string::npos) {
                co.compiler_hash = rest.substr(0, colon);
                rest = rest.substr(colon + 1);
            }

            std::vector<CacheEntry> entries;
            size_t pos = 0;
            while (pos < rest.size()) {
                auto comma = rest.find(',', pos);
                if (comma == std::string::npos) break;

                auto hash_start = rest.find_first_not_of(',', pos);
                if (hash_start == std::string::npos) break;
                /* hash is "sha256-" prefix + 64 hex chars = 71 chars */
                size_t hash_len = (rest.find(',', hash_start) != std::string::npos)
                    ? rest.find(',', hash_start) - hash_start
                    : rest.size() - hash_start;
                std::string hash = rest.substr(hash_start, hash_len);
                pos = hash_start + hash_len;

                if (pos >= rest.size() || rest[pos] != ',') break;
                pos++;
                comma = rest.find(',', pos);
                if (comma == std::string::npos) break;
                int64_t file_sz = 0;
                try { file_sz = std::stoll(rest.substr(pos, comma - pos)); }
                catch (...) { break; }
                pos = comma + 1;

                comma = rest.find(',', pos);
                if (comma == std::string::npos) break;
                int64_t mtime = 0;
                try { mtime = std::stoll(rest.substr(pos, comma - pos)); }
                catch (...) { break; }
                pos = comma + 1;

                entries.push_back({hash, file_sz, mtime});
            }
            if (!entries.empty()) {
                co.deps = entries;
                cache_[output] = co;
            }
        }
    }

    void save() {
        auto cache_path = cache_dir_ + "/cache.txt";
        std::ofstream f(cache_path);
        if (!f) return;
        for (auto& [output, cached] : cache_) {
            f << output << "=" << cached.flags_hash << ":" << cached.compiler_hash << ":";
            for (auto& e : cached.deps) {
                f << e.hash << "," << e.file_size << "," << e.mtime << ",";
            }
            f << "\n";
        }
    }
};
