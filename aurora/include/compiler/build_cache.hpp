#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <fstream>
#include <filesystem>
#include <cstdint>
#include <optional>

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

    bool is_up_to_date(const std::string& file_path, const std::vector<std::string>& deps, const std::string& output_path) {
        if (!fs::exists(output_path)) return false;
        auto it = cache_.find(output_path);
        if (it == cache_.end()) return false;
        auto& prev = it->second;
        if (prev.size() != deps.size()) return false;

        for (size_t i = 0; i < deps.size(); i++) {
            auto current = compute_entry(deps[i]);
            if (!current.has_value()) return false;
            if (current->hash != prev[i].hash ||
                current->file_size != prev[i].file_size ||
                current->mtime != prev[i].mtime) {
                return false;
            }
        }
        return true;
    }

    void mark_cached(const std::string& output_path, const std::vector<std::string>& deps) {
        auto& entries = cache_[output_path];
        entries.clear();
        for (auto& dep : deps) {
            auto entry = compute_entry(dep);
            if (entry.has_value()) {
                entries.push_back(*entry);
            }
        }
        save();
    }

    static std::string compute_hash(const std::string& path) {
        std::string result(64, '0');
        std::ifstream f(path, std::ios::binary | std::ios::ate);
        if (!f) return result;
        auto size = f.tellg();
        f.seekg(0);

        uint64_t h = 0;
        char buf[8192];
        while (f) {
            f.read(buf, sizeof(buf));
            std::streamsize n = f.gcount();
            for (std::streamsize i = 0; i < n; i++) {
                h = h * 16777619u ^ static_cast<uint8_t>(buf[i]);
            }
        }
        h ^= static_cast<uint64_t>(size);
        for (int i = 0; i < 16; i++) {
            auto nibble = (h >> (i * 4)) & 0xF;
            result[i] = nibble < 10 ? '0' + nibble : 'a' + nibble - 10;
        }
        return result;
    }

private:
    std::string cache_dir_;
    std::unordered_map<std::string, std::vector<CacheEntry>> cache_;

    std::optional<CacheEntry> compute_entry(const std::string& path) {
        std::error_code ec;
        auto mtime = fs::last_write_time(path, ec);
        if (ec) return std::nullopt;
        auto size = fs::file_size(path, ec);
        if (ec) return std::nullopt;
        return CacheEntry{compute_hash(path), static_cast<int64_t>(size), mtime.time_since_epoch().count()};
    }

    void load() {
        auto cache_path = cache_dir_ + "/cache.txt";
        std::ifstream f(cache_path);
        if (!f) return;
        std::string line;
        while (std::getline(f, line)) {
            auto eq = line.find('=');
            if (eq == std::string::npos) continue;
            std::string output = line.substr(0, eq);
            std::string rest = line.substr(eq + 1);
            std::vector<CacheEntry> entries;
            size_t pos = 0;
            while (pos < rest.size()) {
                auto comma = rest.find(',', pos);
                if (comma == std::string::npos) break;
                std::string hash = rest.substr(pos, 16);
                pos = comma + 1;
                comma = rest.find(',', pos);
                if (comma == std::string::npos) break;
                int64_t size = std::stoll(rest.substr(pos, comma - pos));
                pos = comma + 1;
                comma = rest.find(',', pos);
                if (comma == std::string::npos) break;
                int64_t mtime = std::stoll(rest.substr(pos, comma - pos));
                pos = comma + 1;
                entries.push_back({hash, size, mtime});
            }
            if (!entries.empty())
                cache_[output] = entries;
        }
    }

    void save() {
        auto cache_path = cache_dir_ + "/cache.txt";
        std::ofstream f(cache_path);
        if (!f) return;
        for (auto& [output, entries] : cache_) {
            f << output << "=";
            for (auto& e : entries) {
                f << e.hash << "," << e.file_size << "," << e.mtime << ",";
            }
            f << "\n";
        }
    }
};
