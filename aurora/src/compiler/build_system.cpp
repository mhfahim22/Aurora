/* ════════════════════════════════════════════════════════════
   build_system.cpp — Parallel compilation and build orchestration
   ════════════════════════════════════════════════════════════ */

#ifndef NOMINMAX
#define NOMINMAX
#endif

// LLVM includes must come before local headers to avoid macro conflicts
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/TargetParser/Host.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Bitcode/BitcodeWriter.h>
#include <llvm/Support/raw_ostream.h>

// LLVM/Windows headers may define 'type' as a macro via win32 headers
#ifdef type
#undef type
#endif

#include <algorithm>

#include "compiler/build_system.hpp"
#include "compiler/build_cache.hpp"
#include "compiler/codegen.hpp"
#include "compiler/optimized_codegen.hpp"
#include "compiler/lexer.hpp"
#include "compiler/parser.hpp"
#include "compiler/memory_analyzer.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <iostream>
#include <thread>
#include <mutex>
#include <atomic>
#include <filesystem>

namespace fs = std::filesystem;

/* ═══════════════════════════════════════════════════════════════
   Cross-compilation target machine creation
   ═══════════════════════════════════════════════════════════════ */

llvm::TargetMachine* create_target_machine(llvm::Module* module, const BuildConfig& cfg) {
    std::string triple = cfg.target_triple.empty()
        ? llvm::sys::getProcessTriple()
        : cfg.target_triple;
    module->setTargetTriple(llvm::Triple(triple));

    std::string error;
    const llvm::Target* target = llvm::TargetRegistry::lookupTarget(triple, error);
    if (!target) {
        std::cerr << "aurora: " << error << "\n";
        return nullptr;
    }

    llvm::TargetOptions options;
    if (cfg.fast_math) {
        options.AllowFPOpFusion = llvm::FPOpFusion::Fast;
        options.UnsafeFPMath = true;
        options.NoInfsFPMath = true;
        options.NoNaNsFPMath = true;
    }

    std::string cpu = cfg.target_triple.empty()
        ? llvm::sys::getHostCPUName().str()
        : "generic";

    std::string features_str;
    if (cfg.target_triple.empty()) {
        auto host_features = llvm::sys::getHostCPUFeatures();
        for (const auto& f : host_features) {
            if (!features_str.empty()) features_str += ",";
            features_str += f.second ? "+" : "-";
            features_str += f.first();
        }
    }

    return target->createTargetMachine(
        triple, cpu, features_str, options, llvm::Reloc::PIC_);
}

/* ═══════════════════════════════════════════════════════════════
   Emit object or bitcode file (cross-compile aware)
   ═══════════════════════════════════════════════════════════════ */

bool emit_object(llvm::Module* module, const std::string& obj_path, const BuildConfig& cfg) {
    if (cfg.use_lto) {
        std::error_code ec;
        llvm::raw_fd_ostream bc_file(obj_path, ec, llvm::sys::fs::OF_None);
        if (ec) {
            std::cerr << "aurora: cannot write bitcode: " << ec.message() << "\n";
            return false;
        }
        llvm::WriteBitcodeToFile(*module, bc_file);
        bc_file.close();
        return true;
    }

    auto* tm = create_target_machine(module, cfg);
    if (!tm) return false;

    module->setDataLayout(tm->createDataLayout());

    std::error_code ec;
    llvm::raw_fd_ostream obj_file(obj_path, ec, llvm::sys::fs::OF_None);
    if (ec) {
        std::cerr << "aurora: cannot write object: " << ec.message() << "\n";
        delete tm;
        return false;
    }

    llvm::legacy::PassManager pm;
    if (tm->addPassesToEmitFile(pm, obj_file, nullptr,
            llvm::CodeGenFileType::ObjectFile)) {
        std::cerr << "aurora: target cannot emit object\n";
        delete tm;
        return false;
    }

    pm.run(*module);
    delete tm;
    return true;
}

/* ═══════════════════════════════════════════════════════════════
   Build worker — compiles a single source file
   ═══════════════════════════════════════════════════════════════ */

static std::string read_file_str(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return "";
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

BuildResult build_source(const std::string& source_file, const BuildConfig& cfg) {
    BuildResult result;

    try {
        std::string source = read_file_str(source_file);
        if (source.empty()) {
            result.message = "cannot open: " + source_file;
            return result;
        }

        /* Lex */
        Lexer lexer;
        auto lines = lexer.lex(source);

        /* Parse */
        Parser parser(lines);
        auto ast = parser.parse();
        if (parser.had_error()) {
            result.message = "parse errors in " + source_file;
            return result;
        }

        /* Memory analysis */
        MemoryAnalyzer memory_analyzer;
        memory_analyzer.analyse(ast.get());
        memory_analyzer.apply_to_ast(ast.get());
        if (memory_analyzer.has_errors()) {
            result.message = "memory analysis errors in " + source_file;
            return result;
        }

        /* Codegen */
        auto ctx = std::make_unique<llvm::LLVMContext>();
        auto module = std::make_unique<llvm::Module>("build_unit", *ctx);
        auto builder = std::make_unique<llvm::IRBuilder<>>(*ctx);

        if (cfg.fast_math) {
            llvm::FastMathFlags fmf;
            fmf.setFast();
            builder->setFastMathFlags(fmf);
        }

        if (cfg.use_optimized_codegen) {
            OptimizedCodegen opt_cg(*ctx, module, builder);
            opt_cg.generate(ast.get(), memory_analyzer);
        } else {
            Codegen codegen(*ctx, module, builder);
            codegen.set_source_file(source_file);
            codegen.set_coverage_enabled(cfg.enable_coverage);
            std::unique_ptr<llvm::DIBuilder> debug_builder;
            if (cfg.enable_debug && !source_file.empty()) {
                debug_builder = std::make_unique<llvm::DIBuilder>(*module);
                codegen.set_debug_builder(debug_builder.get());
                codegen.set_debug_enabled(true);
            }
            codegen.generate(ast.get());
        }

        /* Determine output path */
        std::string obj_path;
        if (!cfg.output_path.empty()) {
            obj_path = cfg.output_path;
        } else {
            auto sep = source_file.find_last_of("/\\");
            std::string base = (sep == std::string::npos)
                ? source_file
                : source_file.substr(sep + 1);
            base = base.substr(0, base.rfind('.'));
            obj_path = base + ".obj";
        }

        /* Emit */
        if (!emit_object(module.get(), obj_path, cfg)) {
            result.message = "emission failed for " + source_file;
            return result;
        }

        result.success = true;
        result.obj_path = obj_path;
        result.message = "compiled " + source_file;

    } catch (const std::exception& e) {
        result.message = std::string("error: ") + e.what();
    } catch (...) {
        result.message = "unknown error compiling " + source_file;
    }

    return result;
}

/* ═══════════════════════════════════════════════════════════════
   Parallel build — compile multiple .aura files across threads
   ═══════════════════════════════════════════════════════════════ */

int build_parallel(const std::vector<std::string>& source_files, const BuildConfig& cfg) {
    int nfiles = (int)source_files.size();
    if (nfiles == 0) {
        std::cerr << "aurora: no source files to compile\n";
        return 1;
    }

    int nthreads = std::max(1, std::min(cfg.jobs, nfiles));
    if (cfg.verbose)
        std::cerr << "aurora: building " << nfiles << " files with "
                  << nthreads << " threads\n";

    std::atomic<int> next_file{0};
    std::atomic<int> errors{0};
    std::vector<BuildResult> results(nfiles);
    std::mutex result_mutex;

    auto worker = [&]() {
        while (true) {
            int idx = next_file.fetch_add(1);
            if (idx >= nfiles) break;

            auto r = build_source(source_files[idx], cfg);
            {
                std::lock_guard<std::mutex> lock(result_mutex);
                results[idx] = r;
                if (!r.success) errors++;
            }

            if (cfg.verbose)
                std::cerr << "  [" << (idx + 1) << "/" << nfiles << "] "
                          << r.message << "\n";
        }
    };

    std::vector<std::thread> threads;
    threads.reserve(nthreads);
    for (int i = 0; i < nthreads; i++)
        threads.emplace_back(worker);
    for (auto& t : threads)
        t.join();

    if (errors > 0) {
        std::cerr << "aurora: " << errors << " file(s) failed to compile\n";
        for (auto& r : results) {
            if (!r.success)
                std::cerr << "  " << r.message << "\n";
        }
        return 1;
    }

    if (cfg.verbose)
        std::cerr << "aurora: all files compiled successfully\n";

    return 0;
}
