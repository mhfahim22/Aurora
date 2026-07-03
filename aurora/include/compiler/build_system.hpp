#pragma once
#include <string>
#include <vector>

struct BuildConfig {
    int jobs = 1;
    std::string target_triple;
    std::string output_path;
    std::vector<std::string> link_libs;
    std::vector<std::string> lib_paths;
    bool emit_obj = false;
    bool emit_ir = false;
    bool run_jit = false;
    bool use_lto = false;
    bool enable_debug = false;
    bool enable_coverage = false;
    bool incremental = false;
    bool fast_math = false;
    bool verbose = false;
    bool use_aurora_ir = false;
    bool use_optimized_codegen = false;
    int opt_level = 2;
    bool opt_size = false;
    bool opt_size_aggressive = false;
    std::string exe_dir;
    std::string source_path;
};

struct BuildResult {
    bool success = false;
    std::string obj_path;
    std::string message;
};

BuildResult build_source(const std::string& source_file, const BuildConfig& cfg);

int build_parallel(const std::vector<std::string>& source_files, const BuildConfig& cfg);
