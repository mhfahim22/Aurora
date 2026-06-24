#include "voss.h"
#include "runtime/interop/universal_binding_gen.hpp"
#include "runtime/interop/eco_type_ir_mapper.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>
namespace fs = std::filesystem;

/* ── Helpers ── */

static std::string read_file_string(const std::string& path) {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) return "";
    std::ostringstream oss;
    oss << ifs.rdbuf();
    return oss.str();
}

static bool is_bindgen_available(std::string& out_path, bool verbose) {
    /* Check in the current build directory first */
    std::vector<std::string> candidates;
    const char* build_dir = std::getenv("AURORA_BUILD_DIR");
    if (build_dir) {
        candidates.push_back(std::string(build_dir) + "/aurora_bindgen.exe");
        candidates.push_back(std::string(build_dir) + "/aurora_bindgen");
    }
    candidates.push_back("aurora_bindgen.exe");
    candidates.push_back("aurora_bindgen");
    candidates.push_back("./build/aurora_bindgen.exe");
    candidates.push_back("./build/aurora_bindgen");
    candidates.push_back("./build/Debug/aurora_bindgen.exe");
    candidates.push_back("./build/Release/aurora_bindgen.exe");

    for (auto& c : candidates) {
        if (fs::exists(c)) {
            out_path = fs::absolute(c).string();
            if (verbose) std::cout << "[bind] found aurora-bindgen at: " << out_path << "\n";
            return true;
        }
    }

    /* Check PATH */
#ifdef _WIN32
    const char* path_env = std::getenv("PATH");
    if (path_env) {
        std::string paths(path_env);
        size_t start = 0, end;
        while ((end = paths.find(';', start)) != std::string::npos) {
            std::string dir = paths.substr(start, end - start);
            start = end + 1;
            if (dir.empty()) continue;
            std::string full = dir + "/aurora_bindgen.exe";
            if (fs::exists(full)) { out_path = full; if (verbose) std::cout << "[bind] found in PATH: " << full << "\n"; return true; }
        }
        std::string last = paths.substr(start);
        if (!last.empty()) {
            std::string full = last + "/aurora_bindgen.exe";
            if (fs::exists(full)) { out_path = full; if (verbose) std::cout << "[bind] found in PATH: " << full << "\n"; return true; }
        }
    }
#else
    const char* path_env = std::getenv("PATH");
    if (path_env) {
        std::string paths(path_env);
        size_t start = 0, end;
        while ((end = paths.find(':', start)) != std::string::npos) {
            std::string dir = paths.substr(start, end - start);
            start = end + 1;
            std::string full = dir + "/aurora_bindgen";
            if (fs::exists(full)) { out_path = full; return true; }
        }
    }
#endif

    return false;
}

/* Known header auto-detection for common libraries */
struct KnownLib {
    const char* name;
    const char* header;
    const char* lib;
};
static KnownLib s_known_libs[] = {
    {"opencv", "opencv2/opencv.hpp", "opencv_world4"},
    {"opencv2", "opencv2/opencv.hpp", "opencv_world4"},
    {"opencv4", "opencv2/opencv.hpp", "opencv_world4"},
    {"sdl2", "SDL2/SDL.h", "SDL2"},
    {"sdl", "SDL2/SDL.h", "SDL2"},
    {"glfw", "GLFW/glfw3.h", "glfw3"},
    {"glfw3", "GLFW/glfw3.h", "glfw3"},
    {"zlib", "zlib.h", "z"},
    {"libpng", "png.h", "png16"},
    {"libjpeg", "jpeglib.h", "jpeg"},
    {"curl", "curl/curl.h", "libcurl"},
    {"libcurl", "curl/curl.h", "libcurl"},
    {"ssl", "openssl/ssl.h", "ssl"},
    {"openssl", "openssl/ssl.h", "ssl"},
    {"crypto", "openssl/crypto.h", "crypto"},
    {"sqlite", "sqlite3.h", "sqlite3"},
    {"sqlite3", "sqlite3.h", "sqlite3"},
    {"pthread", "pthread.h", "pthread"},
    {"dl", "dlfcn.h", "dl"},
    {"m", "math.h", "m"},
    {"rt", "time.h", "rt"},
    {"uuid", "uuid/uuid.h", "uuid"},
    {"freetype", "freetype/freetype.h", "freetype"},
    {"fontconfig", "fontconfig/fontconfig.h", "fontconfig"},
    {"python", "Python.h", "python3"},
    {"python3", "Python.h", "python3"},
    {"lua", "lua.h", "lua5.4"},
    {"lua5.4", "lua.h", "lua5.4"},
    {"luajit", "luajit.h", "luajit"},
    {"ffmpeg", "libavcodec/avcodec.h", "avcodec"},
    {"cairo", "cairo/cairo.h", "cairo"},
    {"gtk", "gtk/gtk.h", "gtk-3"},
    {"gtk3", "gtk/gtk.h", "gtk-3"},
    {"gtk4", "gtk4/gtk.h", "gtk-4"},
    {"qt5", "QtCore/QtCore", "Qt5Core"},
    {"qt6", "QtCore/QtCore", "Qt6Core"},
    {"boost", "boost/version.hpp", "boost"},
    {"nng", "nng/nng.h", "nng"},
    {"zmq", "zmq.h", "zmq"},
    {"libusb", "libusb-1.0/libusb.h", "usb-1.0"},
    {"hidapi", "hidapi/hidapi.h", "hidapi"},
    {"portaudio", "portaudio.h", "portaudio"},
    {"rtaudio", "rtaudio/RtAudio.h", "rtaudio"},
    {"flac", "FLAC/stream_decoder.h", "FLAC"},
    {"vorbis", "vorbis/codec.h", "vorbis"},
    {"ogg", "ogg/ogg.h", "ogg"},
    {"theora", "theora/theora.h", "theora"},
    {"tiff", "tiff.h", "tiff"},
    {"webp", "webp/decode.h", "webp"},
    {"gif", "gif_lib.h", "gif"},
    {"xml2", "libxml/parser.h", "xml2"},
    {"libxml2", "libxml/parser.h", "xml2"},
    {"yaml", "yaml.h", "yaml"},
    {"libyaml", "yaml.h", "yaml"},
    {"sodium", "sodium.h", "sodium"},
    {"libsodium", "sodium.h", "sodium"},
    {nullptr, nullptr, nullptr}
};

static bool find_header(const std::string& header, std::string& out_path, bool verbose) {
    /* Common include search paths */
    std::vector<std::string> search_paths;
#ifdef _WIN32
    const char* vcpkg = std::getenv("VCPKG_ROOT");
    if (vcpkg) {
        search_paths.push_back(std::string(vcpkg) + "/installed/x64-windows/include");
        search_paths.push_back(std::string(vcpkg) + "/installed/x64-windows-static/include");
    }
    search_paths.push_back("C:/Program Files (x86)/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.40.33807/include");
    search_paths.push_back("C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.40.33807/include");
    search_paths.push_back("C:/Program Files (x86)/Windows Kits/10/Include/10.0.26100.0");
    search_paths.push_back("C:/Program Files (x86)/Windows Kits/10/Include/10.0.22621.0");
    search_paths.push_back("C:/Program Files (x86)/Windows Kits/10/Include/10.0.22000.0");
    search_paths.push_back("C:/Program Files (x86)/Windows Kits/10/Include/10.0.20348.0");
    search_paths.push_back("C:/tools/vcpkg/installed/x64-windows/include");
    search_paths.push_back("C:/vcpkg/installed/x64-windows/include");
    const char* local = std::getenv("LOCALAPPDATA");
    if (local) {
        search_paths.push_back(std::string(local) + "/vcpkg/installed/x64-windows/include");
        search_paths.push_back(std::string(local) + "/Programs/Common/include");
    }
#else
    search_paths.push_back("/usr/include");
    search_paths.push_back("/usr/local/include");
    search_paths.push_back("/opt/homebrew/include");
    search_paths.push_back("/opt/local/include");
#endif

    for (auto& base : search_paths) {
        std::string full = base + "/" + header;
        if (fs::exists(full)) {
            out_path = full;
            if (verbose) std::cout << "[bind] found header: " << full << "\n";
            return true;
        }
    }
    return false;
}

static std::string capture_cmd(const std::string& cmd) {
    std::string result;
#ifdef _WIN32
    FILE* pipe = _popen(cmd.c_str(), "r");
#else
    FILE* pipe = popen(cmd.c_str(), "r");
#endif
    if (!pipe) return "";
    char buf[4096];
    while (fgets(buf, sizeof(buf), pipe) != nullptr)
        result += buf;
#ifdef _WIN32
    _pclose(pipe);
#else
    pclose(pipe);
#endif
    return result;
}

/* ── cmd_bind: Generate Aurora FFI bindings for a C library ── */
int cmd_bind(const std::string& library,
             const std::vector<std::string>& headers,
             const std::string& output_dir,
             const std::string& lib_name,
             const std::string& package,
             const std::vector<std::string>& inc_dirs,
             const std::vector<std::string>& defs,
             const std::string& call_conv,
             bool no_cache,
             bool verbose,
             bool no_macros,
             bool no_functions,
             bool no_structs,
             bool no_unions,
             bool no_typedefs) {
    /* 1. Check that aurora-bindgen is available; fallback to template binding */
    std::string bindgen_path;
    bool have_bindgen = is_bindgen_available(bindgen_path, verbose);
    if (!have_bindgen) {
        std::cout << "[bind] aurora-bindgen not found — generating template binding instead\n";
        std::cout << "[bind] install libclang and rebuild for precise per-function bindings\n";
    }

    /* 2. Resolve headers */
    std::vector<std::string> resolved_headers;
    if (!headers.empty()) {
        /* Headers specified as arguments — use as-is if they exist, or search for them */
        for (auto& h : headers) {
            if (fs::exists(h)) {
                resolved_headers.push_back(fs::absolute(h).string());
            } else {
                std::string found;
                if (find_header(h, found, verbose)) {
                    resolved_headers.push_back(found);
                } else {
                    std::cerr << "error: header not found: " << h << "\n";
                    return 1;
                }
            }
        }
    } else {
        /* Auto-detect headers for known libraries */
        std::string lib_lower = library;
        std::transform(lib_lower.begin(), lib_lower.end(), lib_lower.begin(), ::tolower);
        bool found_known = false;
        for (int i = 0; s_known_libs[i].name; i++) {
            if (lib_lower == s_known_libs[i].name) {
                std::string hdr_path;
                if (find_header(s_known_libs[i].header, hdr_path, verbose)) {
                    resolved_headers.push_back(hdr_path);
                    found_known = true;
                    if (verbose) std::cout << "[bind] auto-detected header: " << s_known_libs[i].header << "\n";
                } else {
                    std::cerr << "error: could not find header '" << s_known_libs[i].header
                              << "' for library '" << library << "'\n";
                    std::cerr << "  try: voss bind " << library << " /path/to/header.h\n";
                    return 1;
                }
                break;
            }
        }
        if (!found_known) {
            /* Try <library>.h and <library>/<library>.h */
            for (auto& candidate : {library + ".h", library + "/" + library + ".h"}) {
                std::string found;
                if (find_header(candidate, found, verbose)) {
                    resolved_headers.push_back(found);
                    found_known = true;
                    if (verbose) std::cout << "[bind] auto-detected header: " << candidate << "\n";
                    break;
                }
            }
            if (!found_known) {
                std::cerr << "error: unknown library '" << library
                          << "' — specify header file(s) explicitly\n";
                std::cerr << "  usage: voss bind " << library << " /path/to/header.h\n";
                return 1;
            }
        }
    }

    /* 3. Create output directory */
    fs::path out_dir = fs::absolute(output_dir);
    if (!fs::exists(out_dir))
        fs::create_directories(out_dir);

    /* 4. Compute cache key from header contents */
    std::string cache_key;
    if (!no_cache) {
        std::string combined;
        for (auto& h : resolved_headers) {
            combined += read_file_string(h);
        }
        cache_key = "bind_" + sha256_hex(combined);
    }

    std::string output_file = (out_dir / (library + ".auf")).string();

    /* 5. Check cache */
    if (!no_cache && !cache_key.empty()) {
        std::string cached = cache_get(cache_key);
        if (!cached.empty()) {
            /* Cache hit: write cached content to output file */
            if (verbose) std::cout << "[bind] cache hit for " << library << "\n";
            std::ofstream ofs(output_file);
            if (!ofs) {
                std::cerr << "error: could not write " << output_file << "\n";
                return 1;
            }
            ofs << cached;
            ofs.close();
            std::cout << "wrote " << output_file << " (cached)\n";
            return 0;
        }
        if (verbose) std::cout << "[bind] cache miss for " << library << "\n";
    }

    if (have_bindgen) {
        /* 6a. Build aurora-bindgen command */
        auto q = [](const std::string& s) {
            return (s.find(' ') != std::string::npos || s.find('\t') != std::string::npos)
                ? "\"" + s + "\"" : s;
        };
        std::string cmd = q(bindgen_path);
        for (auto& h : resolved_headers) {
            cmd += " " + q(h);
        }
        cmd += " -o " + q(output_file);
        cmd += " -l " + q(lib_name.empty() ? library : lib_name);
        if (!package.empty()) {
            cmd += " --package " + q(package);
        }
        if (!call_conv.empty()) {
            cmd += " --cc " + q(call_conv);
        }
        for (auto& inc : inc_dirs) {
            cmd += " -I " + q(inc);
        }
        for (auto& d : defs) {
            cmd += " -D " + q(d);
        }
        if (no_macros) cmd += " --no-macros";
        if (no_functions) cmd += " --no-functions";
        if (no_structs) cmd += " --no-structs";
        if (no_unions) cmd += " --no-unions";
        if (no_typedefs) cmd += " --no-typedefs";
        if (verbose) cmd += " --verbose";

        if (verbose) {
            std::cout << "[bind] running: " << cmd << "\n";
        }

        /* 7a. Run bindgen */
        int rc = std::system(cmd.c_str());
        if (rc != 0) {
            std::cerr << "error: aurora-bindgen failed with exit code " << rc << "\n";
            return rc;
        }
    } else {
        /* 6b. Fallback: generate template binding via UniversalBindingGenerator */
        EcosystemTypeIRMapper ir_mapper;
        BindingGenOptions bopts;
        bopts.include_cost_annotations = true;
        bopts.include_marshal_stubs = true;
        UniversalBindingGenerator uni_gen(ir_mapper, bopts);

        UnifiedPackageInfo uinfo;
        uinfo.name = library;
        uinfo.version = "1.0.0";
        uinfo.ecosystem = Ecosystem::Native;
        uinfo.description = "C library: " + library;

        std::string binding = uni_gen.generate(uinfo);
        std::ofstream ofs(output_file);
        if (!ofs) {
            std::cerr << "error: could not write " << output_file << "\n";
            return 1;
        }
        ofs << binding;
        ofs.close();
        std::cout << "[bind] generated template binding for " << library << "\n";
    }

    /* 8. Cache result (aurora-bindgen only; template bindings not cached) */
    if (have_bindgen && !no_cache && !cache_key.empty()) {
        std::string content = read_file_string(output_file);
        if (!content.empty()) {
            cache_put(cache_key, content);
            if (verbose) std::cout << "[bind] cached binding for " << library << "\n";
        }
    }

    std::cout << "wrote " << output_file << "\n";
    return 0;
}
