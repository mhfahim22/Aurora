#include "compiler/codegen.hpp"
#include "compiler/codegen_builtins.hpp"
#include "compiler/typechecker.hpp"
#include "compiler/class_oop.hpp"
#include "compiler/lexer.hpp"
#include "compiler/parser.hpp"
#include "compiler/memory_analyzer.hpp"
#include "compiler/aurora_optimizer.hpp"

#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/TargetParser/Host.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Passes/StandardInstrumentations.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/ADT/StringMap.h>
#include <llvm/Transforms/Scalar/IndVarSimplify.h>
#include <llvm/Transforms/Scalar/SimplifyCFG.h>

#include <fstream>
#include <sstream>
#include <iostream>
#include <memory>

/* ── LLVM initialization guard ── */
bool llvm_initialized = false;

void ensure_llvm_init() {
    if (llvm_initialized) return;
#if defined(__x86_64__) || defined(_M_X64) || defined(i386) || defined(__i386__) || defined(_M_IX86)
    LLVMInitializeX86TargetInfo();
    LLVMInitializeX86Target();
    LLVMInitializeX86TargetMC();
    LLVMInitializeX86AsmPrinter();
    LLVMInitializeX86AsmParser();
#elif defined(__aarch64__) || defined(__ARM64__) || defined(_M_ARM64)
    LLVMInitializeAArch64TargetInfo();
    LLVMInitializeAArch64Target();
    LLVMInitializeAArch64TargetMC();
    LLVMInitializeAArch64AsmPrinter();
    LLVMInitializeAArch64AsmParser();
#else
    LLVMInitializeAllTargetInfos();
    LLVMInitializeAllTargets();
    LLVMInitializeAllTargetMCs();
    LLVMInitializeAllAsmPrinters();
    LLVMInitializeAllAsmParsers();
#endif
    llvm_initialized = true;
}

/* ════════════════════════════════════════════════════════════
   LLVMCodegen — LLVM Module-level utilities
   ════════════════════════════════════════════════════════════
   Provides standalone object-file emission from a Module
   and convenience wrappers for the full Aurora -> .obj pipeline.
   ════════════════════════════════════════════════════════════ */

LLVMCodegen::LLVMCodegen()
    : owned_ctx_(std::make_unique<llvm::LLVMContext>())
    , ctx_(owned_ctx_.get())
    , module_(std::make_unique<llvm::Module>("aurora_module", *ctx_))
    , builder_(std::make_unique<llvm::IRBuilder<>>(*ctx_))
{
    ensure_llvm_init();
    auto triple = llvm::sys::getProcessTriple();
    module_->setTargetTriple(triple);
    module_->setDataLayout(llvm_target_data_layout(triple));
}

LLVMCodegen::LLVMCodegen(llvm::LLVMContext& ctx,
                         std::unique_ptr<llvm::Module>& module,
                         std::unique_ptr<llvm::IRBuilder<>>& builder)
    : ctx_external_(true)
    , ctx_(&ctx)
    , module_(std::move(module))
    , builder_(std::move(builder))
{
}

void LLVMCodegen::generate(const ASTNode* root) {
    Codegen codegen(*ctx_, module_, builder_);
    if (!source_file_path_.empty()) {
        codegen.set_source_file(source_file_path_);
        dibuilder_ = std::make_unique<llvm::DIBuilder>(*module_);
        codegen.set_debug_builder(dibuilder_.get());
        codegen.set_debug_enabled(true);
    }
    codegen.generate(root);
}

bool LLVMCodegen::emit_object_file(const std::string& obj_path) {
    ensure_llvm_init();

    std::string error;
    const llvm::Target* target = llvm::TargetRegistry::lookupTarget(
        module_->getTargetTriple(), error);
    if (!target) {
        std::cerr << "LLVMCodegen: " << error << "\n";
        return false;
    }

    llvm::TargetOptions options;
    std::string cpu = llvm::sys::getHostCPUName().str();
    std::string features_str;
#if LLVM_VERSION_MAJOR >= 19
    auto host_features = llvm::sys::getHostCPUFeatures();
    if (!host_features.empty()) {
        for (const auto& f : host_features) {
            if (!features_str.empty()) features_str += ",";
            features_str += f.second ? "+" : "-";
            features_str += f.first();
        }
    }
#else
    llvm::StringMap<bool, llvm::MallocAllocator> host_features;
    if (llvm::sys::getHostCPUFeatures(host_features)) {
        for (const auto& f : host_features) {
            if (!features_str.empty()) features_str += ",";
            features_str += f.second ? "+" : "-";
            features_str += f.first();
        }
    }
#endif

    auto* target_machine = target->createTargetMachine(
        module_->getTargetTriple(), cpu, features_str, options, llvm::Reloc::Model());

    if (!target_machine) {
        std::cerr << "LLVMCodegen: could not create target machine\n";
        return false;
    }

    module_->setDataLayout(target_machine->createDataLayout());

    std::error_code ec;
    llvm::raw_fd_ostream obj_file(obj_path, ec, llvm::sys::fs::OF_None);
    if (ec) {
        std::cerr << "LLVMCodegen: cannot write object file: " << ec.message() << "\n";
        delete target_machine;
        return false;
    }

    llvm::legacy::PassManager pass_manager;
    if (target_machine->addPassesToEmitFile(pass_manager, obj_file, nullptr,
            llvm::CodeGenFileType::ObjectFile)) {
        std::cerr << "LLVMCodegen: target machine cannot emit object file\n";
        delete target_machine;
        return false;
    }

    pass_manager.run(*module_);
    delete target_machine;
    return true;
}

void LLVMCodegen::run_optimization_passes() {
    llvm::LoopAnalysisManager LAM;
    llvm::FunctionAnalysisManager FAM;
    llvm::CGSCCAnalysisManager CGAM;
    llvm::ModuleAnalysisManager MAM;

    llvm::PassBuilder PB;
    PB.registerModuleAnalyses(MAM);
    PB.registerCGSCCAnalyses(CGAM);
    PB.registerFunctionAnalyses(FAM);
    PB.registerLoopAnalyses(LAM);
    PB.crossRegisterProxies(LAM, FAM, CGAM, MAM);

    llvm::ModulePassManager MPM =
        PB.buildPerModuleDefaultPipeline(llvm::OptimizationLevel::O2);
    MPM.run(*module_, MAM);

    {
        llvm::LoopPassManager LPM;
        LPM.addPass(llvm::IndVarSimplifyPass());

        llvm::FunctionPassManager FPM;
        FPM.addPass(llvm::createFunctionToLoopPassAdaptor(std::move(LPM)));
        FPM.addPass(llvm::SimplifyCFGPass());

        llvm::ModulePassManager MPM2;
        MPM2.addPass(llvm::createModuleToFunctionPassAdaptor(std::move(FPM)));
        MPM2.run(*module_, MAM);
    }
}

bool LLVMCodegen::verify_module() const {
    std::string err_str;
    llvm::raw_string_ostream err_os(err_str);
    if (llvm::verifyModule(*module_, &err_os)) {
        std::cerr << "LLVMCodegen: module verification failed:\n" << err_str << "\n";
        return false;
    }
    return true;
}

void LLVMCodegen::dump_ir(std::ostream& os) const {
    std::string ir_str;
    llvm::raw_string_ostream ir_os(ir_str);
    module_->print(ir_os, nullptr);
    ir_os.flush();
    os << ir_str;
}

bool LLVMCodegen::write_ir(const std::string& path) const {
    std::string ir_str;
    llvm::raw_string_ostream ir_os(ir_str);
    module_->print(ir_os, nullptr);
    ir_os.flush();

    std::ofstream ofs(path);
    if (!ofs.is_open()) {
        std::cerr << "LLVMCodegen: cannot write IR to " << path << "\n";
        return false;
    }
    ofs << ir_str;
    return true;
}

llvm::Module* LLVMCodegen::module() { return module_.get(); }

/* ════════════════════════════════════════════════════════════
   JIT Execution (for REPL) — uses OrcJIT for in-process execution
   ════════════════════════════════════════════════════════════ */
#include <llvm/Support/Error.h>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>
#include <llvm/ExecutionEngine/Orc/ExecutionUtils.h>

/* ── Force linker to include aurora_runtime.lib server.obj symbols ── */
extern "C" void aurora_http_response_set_body_n(void*, const char*, size_t);
extern "C" void aurora_server_static(void*, const char*, const char*);
static void* _jit_dummy_server_syms[2] = {
    (void*)(void*)&aurora_http_response_set_body_n,
    (void*)(void*)&aurora_server_static
};
#include <llvm/Support/DynamicLibrary.h>

static void log_error(llvm::Error err) {
    llvm::handleAllErrors(std::move(err), [](const llvm::ErrorInfoBase& E) {
        std::cerr << "JIT error: " << E.message() << "\n" << std::flush;
    });
}

int jit_execute_main(std::unique_ptr<llvm::LLVMContext> ctx,
                     std::unique_ptr<llvm::Module> module) {
    using namespace llvm;
    using namespace llvm::orc;

    InitializeNativeTarget();
    InitializeNativeTargetAsmPrinter();
    InitializeNativeTargetAsmParser();

    auto jit_or_err = LLJITBuilder().create();
    if (!jit_or_err) {
        log_error(jit_or_err.takeError());
        return -1;
    }
    auto& jit = *jit_or_err;

    /* Register symbols the JIT may need from the runtime via absoluteSymbols */
    {
        auto& JD = jit->getMainJITDylib();
        llvm::orc::SymbolMap sm;
        auto intern = [&](const char* name) {
            return jit->getExecutionSession().intern(name);
        };
        sm[intern("aurora_http_response_set_body_n")] =
            llvm::orc::ExecutorSymbolDef(
                llvm::orc::ExecutorAddr::fromPtr(
                    (void*)(void*)&aurora_http_response_set_body_n),
                llvm::JITSymbolFlags::Exported);
        sm[intern("aurora_server_static")] =
            llvm::orc::ExecutorSymbolDef(
                llvm::orc::ExecutorAddr::fromPtr(
                    (void*)(void*)&aurora_server_static),
                llvm::JITSymbolFlags::Exported);
        if (auto err = JD.define(llvm::orc::absoluteSymbols(std::move(sm))))
            log_error(std::move(err));
    }
    /* Use dummy array to keep linker from discarding server.obj */
    (void)_jit_dummy_server_syms[0];
    (void)_jit_dummy_server_syms[1];

    /* Add current process symbols */
    auto gen = DynamicLibrarySearchGenerator::GetForCurrentProcess(
        jit->getDataLayout().getGlobalPrefix());
    if (gen)
        (void)jit->getMainJITDylib().addGenerator(std::move(*gen));

    module->setTargetTriple(jit->getTargetTriple().getTriple());
    module->setDataLayout(jit->getDataLayout());

    auto TSM = ThreadSafeModule(std::move(module), std::move(ctx));
    if (auto err = jit->addIRModule(std::move(TSM))) {
        log_error(std::move(err));
        return -1;
    }

    auto sym = jit->lookup("main");
    if (!sym) {
        log_error(sym.takeError());
        return -1;
    }

    auto* main_fn = (int(*)())(sym->getValue());
    if (!main_fn) {
        std::cerr << "JIT error: main symbol is null\n" << std::flush;
        return -1;
    }

    return main_fn();
}

/* ── Convenience: full .aura -> .obj pipeline ── */
bool compile_aura_to_object(const std::string& source_path,
                            const std::string& obj_path,
                            bool optimize) {
    std::ifstream f(source_path);
    if (!f.is_open()) {
        std::cerr << "cannot open file: " << source_path << "\n";
        return false;
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    std::string source = ss.str();

    Lexer lexer;
    auto lines = lexer.lex(source);

    Parser parser(lines);
    ASTNode::Ptr ast = parser.parse();

    MemoryAnalyzer memory_analyzer;
    memory_analyzer.analyse(ast.get());
    memory_analyzer.apply_to_ast(ast.get());

    if (memory_analyzer.has_errors()) {
        std::cerr << "compilation errors found, stopping\n";
        return false;
    }

    LLVMCodegen codegen;
    codegen.set_source_file_path(source_path);
    codegen.generate(ast.get());

    if (optimize)
        codegen.run_optimization_passes();

    if (!codegen.verify_module())
        return false;

    return codegen.emit_object_file(obj_path);
}
