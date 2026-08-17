# aurora_restructured/CMakeLists.txt (updated)
cmake_minimum_required(VERSION 3.20)
project(Aurora VERSION 1.0.0 LANGUAGES C CXX)
include(CTest)
set(DART_TESTING_TIMEOUT 60)

# ── Read VERSION file and generate version.h ──
file(STRINGS "${CMAKE_SOURCE_DIR}/VERSION" AURORA_VERSION_STR)
string(STRIP "${AURORA_VERSION_STR}" AURORA_VERSION_STR)
string(REPLACE "." ";" AURORA_VERSION_LIST "${AURORA_VERSION_STR}")
list(GET AURORA_VERSION_LIST 0 AURORA_VERSION_MAJOR)
list(GET AURORA_VERSION_LIST 1 AURORA_VERSION_MINOR)
list(GET AURORA_VERSION_LIST 2 AURORA_VERSION_PATCH)
configure_file(
    "${CMAKE_SOURCE_DIR}/aurora/include/runtime/version.h.in"
    "${CMAKE_BINARY_DIR}/include/aurora/runtime/version.h"
    @ONLY
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── AddressSanitizer (optional, off by default) ───────────────
option(ENABLE_ASAN "Enable AddressSanitizer for test targets" OFF)
if(ENABLE_ASAN)
    if(MSVC)
        set(ASAN_FLAGS "/fsanitize=address" "/Oy-" "/Zi")
        set(ASAN_LINK_FLAGS "/DEBUG")
    else()
        set(ASAN_FLAGS "-fsanitize=address" "-fno-omit-frame-pointer")
        set(ASAN_LINK_FLAGS "-fsanitize=address")
    endif()
endif()

# ── Force Release CRT (/MD) for all configs to match LLVM libs ──
# LLVM 18.1.8 at C:\LLVM was built with /MD (Release CRT).
# Mixing /MDd with LLVM's /MD causes LNK2038 RuntimeLibrary mismatch.
# Consequence: Debug builds lose CRT debug checks; to restore them,
# LLVM must be rebuilt from source with /MDd.
if(MSVC)
    cmake_policy(SET CMP0091 NEW)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL" CACHE STRING "")
    add_compile_definitions($<$<CONFIG:Debug>:_ITERATOR_DEBUG_LEVEL=0>)
endif()

# ── Find X11 (Linux GUI) ────────────────────────────────────
if(NOT WIN32 AND NOT APPLE)
    find_package(X11 QUIET)
endif()

# ── Find LLVM ────────────────────────────────────────────────
# MinGW LLVM 21+ cmake configs reference ZLIB::ZLIB
if(WIN32 AND NOT MSVC)
    # Force MinGW LLVM path when compiler is GCC (MinGW)
    set(LLVM_DIR "C:/msys64/ucrt64/lib/cmake/llvm" CACHE PATH "LLVM installation directory" FORCE)
    set(Clang_DIR "C:/msys64/ucrt64/lib/cmake/clang" CACHE PATH "Clang installation directory" FORCE)
    find_package(ZLIB QUIET)
endif()
# Find LLVM ────────────────────────────────────────────────────────
# Mobile cross-compiles only build aurora_runtime (which never uses
# LLVM); the host LLVM config may reference zstd/zlib targets that the
# NDK/iOS SDK cannot resolve, so skip it entirely for those targets.
if(ANDROID OR IOS)
    set(LLVM_FOUND OFF)
    message(STATUS "Mobile cross-compile: LLVM not required (runtime-only build)")
else()
    find_package(LLVM CONFIG)
    if(LLVM_FOUND)
        message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
        include_directories(${LLVM_INCLUDE_DIRS})
        add_definitions(${LLVM_DEFINITIONS})
    else()
        message(STATUS "LLVM not found - compiler and tool targets will be skipped (runtime-only build)")
    endif()
endif()

# ── Global include path ───────────────────────────────────────
include_directories(${CMAKE_SOURCE_DIR}/aurora/include)

# ── Compiler (aurorac) ────────────────────────────────────────
# Only built when LLVM is available (skipped on mobile cross-compiles,
# which build aurora_runtime only).
if(LLVM_FOUND)
add_executable(aurorac
    aurora/src/compiler/main.cpp
    aurora/src/compiler/lexer/lexer.cpp
    aurora/src/compiler/lexer/token.cpp
    aurora/src/compiler/parser/parser.cpp
    aurora/src/compiler/ir/ir.cpp
    aurora/src/compiler/ir/ir_lowering.cpp
    aurora/src/compiler/ir/ir_optimizer.cpp
    aurora/src/compiler/ir/ir_mem2reg.cpp
    aurora/src/compiler/ir/ast_to_ir.cpp
    aurora/src/compiler/parser/parse_stmt.cpp
    aurora/src/compiler/parser/parse_extern.cpp
    aurora/src/compiler/parser/parse_expr.cpp
    aurora/src/compiler/parser/parse_class.cpp
    aurora/src/compiler/parser/parse_struct.cpp
    aurora/src/compiler/parser/parse_union.cpp
    aurora/src/compiler/parser/parse_enum.cpp
    aurora/src/compiler/parser/parse_interface.cpp
    aurora/src/compiler/parser/parse_type_alias.cpp
    aurora/src/compiler/semantic/typechecker.cpp
    aurora/src/compiler/semantic/typechecker_oop.cpp
    aurora/src/compiler/semantic/typechecker_types.cpp
    aurora/src/compiler/semantic/ownership_checker.cpp
    aurora/src/compiler/semantic/memory_analyzer.cpp
    aurora/src/compiler/semantic/escape_analyzer.cpp
    aurora/src/compiler/semantic/lifetime_analyzer.cpp
    aurora/src/compiler/semantic/ownership_analyzer.cpp
    aurora/src/compiler/semantic/allocation_strategy.cpp
    aurora/src/compiler/semantic/allocation_profiler.cpp
    aurora/src/compiler/semantic/benchmark_suite.cpp
    aurora/src/compiler/semantic/module_resolver.cpp
    aurora/src/compiler/codegen/codegen_core.cpp
    aurora/src/compiler/codegen/codegen_runtime.cpp
    aurora/src/compiler/codegen/codegen_stmt.cpp
    aurora/src/compiler/codegen/codegen_ffi.cpp
    aurora/src/compiler/bridge/bridge_codegen.cpp
    aurora/src/compiler/bridge/bridge_python.cpp
    aurora/src/compiler/bridge/bridge_quickjs.cpp
    aurora/src/compiler/bridge/bridge_rust.cpp
    aurora/src/compiler/codegen/codegen_expr.cpp
    aurora/src/compiler/codegen/codegen_try.cpp
    aurora/src/compiler/codegen/codegen_function.cpp
    aurora/src/compiler/codegen/codegen_oop.cpp
    aurora/src/compiler/codegen/codegen_types.cpp
    aurora/src/compiler/codegen/codegen_builtins.cpp
    aurora/src/compiler/codegen/codegen_builtins_section1.cpp
    aurora/src/compiler/codegen/codegen_builtins_section2.cpp
    aurora/src/compiler/codegen/codegen_builtins_section3.cpp
    aurora/src/compiler/codegen/codegen_builtins_section4.cpp
    aurora/src/compiler/codegen/codegen_builtins_section5.cpp
    aurora/src/compiler/codegen/codegen_ai.cpp
    aurora/src/compiler/codegen/llvm_codegen.cpp
    aurora/src/compiler/codegen/optimized_codegen.cpp
    aurora/src/compiler/aurora_optimizer.cpp
    aurora/src/compiler/optimizer/const_fold.cpp
    aurora/src/compiler/optimizer/dead_code.cpp
    aurora/src/compiler/optimizer/strength_reduce.cpp
    aurora/src/compiler/optimizer/loop_idiom.cpp


    aurora/src/compiler/runtime_exports_init.cpp
    aurora/src/compiler/package_manager.cpp
    aurora/src/compiler/doc_generator.cpp
    aurora/src/compiler/build_system.cpp
    aurora/src/compiler/repl.cpp
    "${CMAKE_BINARY_DIR}/generated/linker_include_symbols.cpp"
)

# Auto-generate MSVC linker include directives from runtime_exports.hpp
set(RUNTIME_EXPORTS_HPP "${CMAKE_SOURCE_DIR}/aurora/src/runtime/runtime_exports.hpp")
set(GENERATED_LINKER_INCLUDES "${CMAKE_BINARY_DIR}/generated/linker_include_symbols.cpp")
add_custom_command(
    OUTPUT ${GENERATED_LINKER_INCLUDES}
    COMMAND ${CMAKE_COMMAND}
        -DRUNTIME_EXPORTS_HPP="${RUNTIME_EXPORTS_HPP}"
        -DGENERATED_CPP="${GENERATED_LINKER_INCLUDES}"
        -P "${CMAKE_SOURCE_DIR}/cmake/gen_linker_includes.cmake"
    DEPENDS "${RUNTIME_EXPORTS_HPP}" "${CMAKE_SOURCE_DIR}/cmake/gen_linker_includes.cmake"
    COMMENT "Generating linker include directives from runtime_exports.hpp..."
)
source_group("Generated" FILES "${GENERATED_LINKER_INCLUDES}")

# LLVM components: native + nativecodegen auto-detect the host architecture
# (X86 on x86, AArch64 on ARM64, etc.)
set(AURORA_LLVM_COMPONENTS
    core support irreader passes analysis
    native nativecodegen
    orcjit
)

# WebAssembly backend for the WASM/browser target (Phase 40). Pulls in the
# WebAssembly codegen + asm parser so `aurorac --target wasm32` can emit
# WebAssembly object files. Only enabled when the linked LLVM actually ships
# the WebAssembly target (CI prebuilt LLVMs omit it → otherwise
# llvm_map_components_to_libnames errors with "omitted target").
set(AURORA_HAS_WASM OFF)
if(LLVM_AVAILABLE_LIBS AND "LLVMWebAssemblyCodeGen" IN_LIST LLVM_AVAILABLE_LIBS)
    set(AURORA_HAS_WASM ON)
endif()
if(AURORA_HAS_WASM AND NOT AURORA_NO_WASM)
    list(APPEND AURORA_LLVM_COMPONENTS WebAssembly WebAssemblyAsmParser)
    target_compile_definitions(aurorac PRIVATE AURORA_HAS_WASM=1)
endif()

# Conditionally add ASM parser for the host architecture
# (native/nativecodegen auto-selects CodeGen + AsmPrinter but not AsmParser)
if(CMAKE_SYSTEM_PROCESSOR MATCHES "AMD64|x86_64|X86|x86|amd64")
    list(APPEND AURORA_LLVM_COMPONENTS X86AsmParser)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|ARM64|arm64")
    list(APPEND AURORA_LLVM_COMPONENTS AArch64AsmParser)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm|ARM")
    list(APPEND AURORA_LLVM_COMPONENTS ARMAsmParser)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "riscv64|RISCV64")
    list(APPEND AURORA_LLVM_COMPONENTS RISCVAsmParser)
endif()

llvm_map_components_to_libnames(LLVM_LIBS ${AURORA_LLVM_COMPONENTS})
target_link_libraries(aurorac PRIVATE ${LLVM_LIBS})

target_include_directories(aurorac PRIVATE "${CMAKE_BINARY_DIR}/include")

if(MSVC)
    target_compile_definitions(aurorac PRIVATE NOMINMAX)
endif()

if(UNIX)
    target_link_options(aurorac PRIVATE -rdynamic)
endif()
endif() # LLVM_FOUND

# ═══════════════════════════════════════════════════════════════
# Static Runtime library
# ═══════════════════════════════════════════════════════════════
add_library(aurora_runtime STATIC
    aurora/src/runtime/core/memory.cpp
    aurora/src/runtime/core/leak_detector.cpp
    aurora/src/runtime/core/coverage.cpp
    aurora/src/runtime/core/aurora_str.cpp
    aurora/src/runtime/core/io.cpp
    aurora/src/runtime/core/array.cpp
    aurora/src/runtime/core/string.cpp
    aurora/src/runtime/core/util.cpp
    aurora/src/runtime/core/crash.cpp
    aurora/src/runtime/core/reflection.cpp
    aurora/src/runtime/core/ffi.cpp
    aurora/src/runtime/core/ffi_abi.cpp
    aurora/src/runtime/debug/dap_runtime.cpp
    aurora/src/runtime/wasm/dom.cpp
    aurora/src/runtime/interop/type_ir.cpp
    aurora/src/runtime/interop/type_mapping.cpp
    aurora/src/runtime/interop/type_mapping_langs.cpp
    aurora/src/runtime/interop/type_serializer.cpp
    aurora/src/runtime/interop/bridge_python_runtime.cpp
    aurora/src/runtime/interop/bridge_quickjs_runtime.cpp
    aurora/src/runtime/interop/bridge_rust_runtime.cpp
    aurora/src/runtime/builtins/builtins.cpp
    aurora/src/runtime/builtins/io_builtins.cpp
    aurora/src/runtime/builtins/std_builtins.cpp
    aurora/src/runtime/builtins/backend_builtins.cpp
    aurora/src/runtime/builtins/ai_builtins.cpp
    aurora/src/runtime/builtins/ai_builtins_ai.cpp
    aurora/src/runtime/ai/ai_layers.cpp
    aurora/src/runtime/ai/ai_layers_attention.cpp
    aurora/src/runtime/ai/ai_layers_dense.cpp
    aurora/src/runtime/ai/ai_layers_rnn.cpp
    aurora/src/runtime/ai/ai_layers_moe.cpp
    aurora/src/runtime/ai/ai_train.cpp
    aurora/src/runtime/ai/ai_model.cpp
    aurora/src/runtime/ai/ai_gen.cpp
    aurora/src/runtime/ai/ai_distributed.cpp
    aurora/src/runtime/ai/ai_paged_attention.cpp
    aurora/src/runtime/ai/ai_onnx.cpp
    aurora/src/runtime/ai/ai_loaders.cpp
    aurora/src/runtime/ai/ai_gpu.cpp
    aurora/src/runtime/ai/dml_matmul.cpp
    aurora/src/runtime/ai/ai_speculative.cpp
    aurora/src/runtime/ai/tokenizer.cpp
    aurora/src/runtime/async/task.cpp
    aurora/src/runtime/async/scheduler.cpp
    aurora/src/runtime/async/channel.cpp
    aurora/src/runtime/async/event_bus.cpp
    aurora/src/runtime/async/fiber.cpp
    aurora/src/runtime/async/event_loop.cpp
    aurora/src/runtime/std/collections.cpp
    aurora/src/runtime/ai/tensor.cpp
    aurora/src/runtime/ai/autograd.cpp
    aurora/src/runtime/ai/dataloader.cpp
    aurora/src/runtime/ai/predict.cpp
    aurora/src/runtime/ai/ai_enterprise.cpp
    aurora/src/runtime/ui/render.cpp
    aurora/src/runtime/ui/component.cpp
    aurora/src/runtime/game/engine.cpp
    aurora/src/runtime/gfx/gl_helper.cpp
    aurora/src/runtime/gfx/matrix.cpp
    aurora/src/runtime/gfx/image_helper.cpp
    aurora/src/runtime/gfx/obj_helper.cpp
    aurora/src/runtime/gfx/audio_helper.cpp
    aurora/src/runtime/gfx/sprite2d.cpp
    aurora/src/runtime/backend/server.cpp
    aurora/src/runtime/backend/tls.cpp
    aurora/src/runtime/backend/websocket.cpp
    aurora/src/runtime/backend/database.cpp
    aurora/src/runtime/backend/graphql.cpp
    aurora/src/runtime/backend/gateway.cpp
    aurora/src/runtime/backend/h2_server.cpp
    aurora/src/runtime/backend/webhook.cpp
    aurora/src/runtime/backend/orm.cpp
    aurora/src/runtime/backend/rest.cpp
    aurora/src/runtime/backend/template.cpp
    aurora/src/runtime/backend/miniz.c
    aurora/src/runtime/backend/miniz_tdef.c
    aurora/src/std/app.cpp
    aurora/src/std/app_layout.cpp
    aurora/src/std/app_nav.cpp
    aurora/src/std/app_theme.cpp
    aurora/src/std/widget.cpp
    aurora/src/std/widgets_advanced.cpp
    aurora/src/std/widget_tree_diff.cpp
    aurora/src/std/hot_reload_gui.cpp
    aurora/src/std/inspector.cpp
    aurora/src/std/i18n.cpp
    aurora/src/std/a11y.cpp
    aurora/src/std/math.cpp
    aurora/src/std/json.cpp
    aurora/src/std/fs.cpp
    aurora/src/std/net.cpp
    aurora/src/std/regex.cpp
    aurora/src/std/datetime.cpp
    aurora/src/std/uuid.cpp
    aurora/src/std/url.cpp
    aurora/src/std/decimal.cpp
    aurora/src/std/crypto.cpp
    aurora/src/std/audio.cpp
    aurora/src/std/animation.cpp
    aurora/src/std/app_security.cpp
    aurora/src/std/async.cpp
    aurora/src/std/canvas.cpp
    aurora/src/std/db.cpp
    aurora/src/std/desktop.cpp
    aurora/src/std/dev.cpp
    aurora/src/std/game.cpp
    aurora/src/std/hotreload.cpp
    aurora/src/std/image.cpp
    aurora/src/std/mobile_stubs.cpp
    aurora/src/std/pkg.cpp
    aurora/src/std/plugin.cpp
    aurora/src/std/theme_store.cpp
    aurora/src/std/widget_plugin.cpp
    aurora/src/std/process.cpp
    aurora/src/std/push.cpp
    aurora/src/std/reactive.cpp
    aurora/src/std/store.cpp
    aurora/src/std/telemetry.cpp
    aurora/src/std/updater.cpp
    aurora/src/std/security.cpp
    aurora/src/std/serial.cpp
    aurora/src/std/test.cpp
    aurora/src/std/thread.cpp
    aurora/src/std/video.cpp
    aurora/src/std/virtual_scroll.cpp
    aurora/src/mobile/widgets.cpp
    third_party/sqlite3/sqlite3.c
)

if(WIN32)
    if(MSVC)
        enable_language(ASM_MASM)
        target_sources(aurora_runtime PRIVATE aurora/src/runtime/core/ffi_call_win64.asm)
    else()
        target_sources(aurora_runtime PRIVATE aurora/src/runtime/core/ffi_call_win64_gcc.cpp)
    endif()
endif()

set_source_files_properties(aurora/src/runtime/backend/miniz.c PROPERTIES COMPILE_DEFINITIONS "MINIZ_NO_STDIO;MINIZ_NO_TIME;MINIZ_NO_ARCHIVE_APIS;MINIZ_NO_ZLIB_APIS")
set_source_files_properties(aurora/src/runtime/backend/miniz_tdef.c PROPERTIES COMPILE_DEFINITIONS "MINIZ_NO_STDIO;MINIZ_NO_TIME;MINIZ_NO_ARCHIVE_APIS;MINIZ_NO_ZLIB_APIS")
if(APPLE)
    target_sources(aurora_runtime PRIVATE aurora/src/std/gui_mac.mm)
    target_link_libraries(aurora_runtime PRIVATE
        "-framework Cocoa"
        "-framework WebKit"
        "-framework AVKit"
        "-framework AVFoundation"
        "-framework CoreMedia")
    set_source_files_properties(aurora/src/std/gui_mac.mm PROPERTIES LANGUAGE CXX
        COMPILE_FLAGS "-x objective-c++")
    if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
        target_sources(aurora_runtime PRIVATE
            aurora/src/mobile/ios/ios_runtime.mm
            aurora/src/mobile/ios/ios_renderer.mm
            aurora/src/mobile/ios/ios_renderer_widgets.mm)
        target_link_libraries(aurora_runtime PRIVATE "-framework UIKit" "-framework Metal")
    endif()
else()
    target_sources(aurora_runtime PRIVATE aurora/src/std/gui.cpp
        aurora/src/mobile/desktop_renderer.cpp)
    if(WIN32)
        target_sources(aurora_runtime PRIVATE aurora/src/runtime/ui/ui_win32.cpp)
    else()
        if(X11_FOUND)
            target_link_libraries(aurora_runtime PRIVATE ${X11_LIBRARIES})
            target_include_directories(aurora_runtime PRIVATE ${X11_INCLUDE_DIR})
        endif()
    endif()
endif()

target_include_directories(aurora_runtime PRIVATE ${CMAKE_SOURCE_DIR}/aurora/include
    ${CMAKE_SOURCE_DIR}/aurora/src/runtime/backend
    ${CMAKE_SOURCE_DIR}/third_party
    "${CMAKE_BINARY_DIR}/include")

# ── OpenGL / 3D Graphics support (optional) ──
if(WIN32)
    target_link_libraries(aurora_runtime PRIVATE opengl32.lib)
    target_compile_definitions(aurora_runtime PRIVATE AURORA_GL=1)
elseif(APPLE)
    target_link_libraries(aurora_runtime PRIVATE "-framework OpenGL")
    target_compile_definitions(aurora_runtime PRIVATE AURORA_GL=1)
else()
    find_package(OpenGL QUIET)
    if(OpenGL_FOUND)
        target_link_libraries(aurora_runtime PRIVATE OpenGL::GL)
        target_compile_definitions(aurora_runtime PRIVATE AURORA_GL=1)
    else()
        message(STATUS "OpenGL not found — 3D graphics support disabled")
        target_compile_definitions(aurora_runtime PRIVATE AURORA_GL=0)
    endif()
endif()

# ── Windows-specific libraries (MinGW/GCC cannot use #pragma comment(lib)) ──
if(WIN32 AND NOT MSVC)
    target_link_libraries(aurora_runtime PRIVATE
        dbghelp
        dxgi d3d12 d3d11 d3dcompiler
        crypt32 secur32
        opengl32
        bcrypt
        winhttp ws2_32
        shlwapi comctl32 dwmapi
        uiautomationcore
        winmm)
endif()

# ── AddressSanitizer for runtime library ──
if(ENABLE_ASAN)
    target_compile_options(aurora_runtime PRIVATE ${ASAN_FLAGS})
    target_link_options(aurora_runtime PRIVATE ${ASAN_LINK_FLAGS})
endif()

# ── Disable C++ exceptions for runtime (no try/catch in runtime) ──
# This removes VCRUNTIME140.dll dependency in AOT builds
if(MSVC)
    target_compile_options(aurora_runtime PRIVATE /EHs-c-)
else()
    target_compile_options(aurora_runtime PRIVATE -fno-exceptions)
endif()
# Files that use std::regex or test framework need exceptions
if(MSVC)
    set_source_files_properties(aurora/src/std/regex.cpp PROPERTIES COMPILE_FLAGS "/EHsc")
    set_source_files_properties(aurora/src/std/test.cpp PROPERTIES COMPILE_FLAGS "/EHsc")
else()
    set_source_files_properties(aurora/src/std/regex.cpp PROPERTIES COMPILE_FLAGS "-fexceptions")
    set_source_files_properties(aurora/src/std/test.cpp PROPERTIES COMPILE_FLAGS "-fexceptions")
endif()

# ── LTO (Link-Time Optimization) for runtime library ──
# Compiles runtime .obj files with LLVM bitcode so the linker can perform LTO
# Disable with: -DAURORA_RUNTIME_LTO=OFF (needed for AOT linking with lld-link)
# MinGW ld cannot consume LTO bitcode (LLVM IR in .obj files).
if(WIN32 AND NOT MSVC)
    set(AURORA_RUNTIME_LTO_DEFAULT OFF)
else()
    set(AURORA_RUNTIME_LTO_DEFAULT ON)
endif()
option(AURORA_RUNTIME_LTO "Enable LTO for aurora_runtime static library" ${AURORA_RUNTIME_LTO_DEFAULT})
if(AURORA_RUNTIME_LTO)
  set_property(TARGET aurora_runtime PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

# ── Link runtime to compiler ────────────────
if(TARGET aurorac)
target_link_libraries(aurorac PRIVATE aurora_runtime)
# MinGW: export runtime symbols for ORC JIT DynamicLibrarySearchGenerator
if(WIN32 AND NOT MSVC)
    set(AURORA_DEF_FILE "${CMAKE_BINARY_DIR}/generated/aurora_exports.def")
    add_custom_command(
        OUTPUT ${AURORA_DEF_FILE}
        COMMAND ${CMAKE_COMMAND}
            -DRUNTIME_EXPORTS_HPP="${RUNTIME_EXPORTS_HPP}"
            -DGENERATED_DEF="${AURORA_DEF_FILE}"
            -P "${CMAKE_SOURCE_DIR}/cmake/gen_export_def.cmake"
        DEPENDS "${RUNTIME_EXPORTS_HPP}" "${CMAKE_SOURCE_DIR}/cmake/gen_export_def.cmake"
        COMMENT "Generating MinGW .def file from runtime_exports.hpp..."
    )
    add_custom_target(aurora_exports_def DEPENDS ${AURORA_DEF_FILE})
    # Pass .def file to linker so its symbols get exported.
    # Use target_link_libraries rather than -Wl,<path> so paths
    # containing spaces (e.g. "D:/Aurora Lang/...") are quoted correctly.
    target_link_libraries(aurorac PRIVATE "${AURORA_DEF_FILE}")
    add_dependencies(aurorac aurora_exports_def)
endif()
endif() # TARGET aurorac

# ═══════════════════════════════════════════════════════════════
# Optional: CUDA support
# ═══════════════════════════════════════════════════════════════
find_package(CUDAToolkit QUIET)
if(CUDAToolkit_FOUND)
    message(STATUS "Found CUDA ${CUDAToolkit_VERSION}")
    enable_language(CUDA)
    target_sources(aurora_runtime PRIVATE aurora/src/runtime/ai/cuda_matmul.cu aurora/src/runtime/ai/cuda_elewise.cu)
    target_include_directories(aurora_runtime PRIVATE ${CUDAToolkit_INCLUDE_DIRS})
    set_property(TARGET aurora_runtime PROPERTY CUDA_ARCHITECTURES "75;80;86;89")
    target_compile_definitions(aurora_runtime PRIVATE AURORA_CUDA=1)
else()
    message(STATUS "CUDA not found — building without GPU matmul")
    target_compile_definitions(aurora_runtime PRIVATE AURORA_CUDA=0)
endif()

# ═══════════════════════════════════════════════════════════════
# Optional: ROCm/HIP support
# ═══════════════════════════════════════════════════════════════
find_package(hip QUIET)
if(hip_FOUND)
    message(STATUS "Found ROCm/HIP")
    target_sources(aurora_runtime PRIVATE aurora/src/runtime/ai/hip_matmul.hip)
    target_include_directories(aurora_runtime PRIVATE ${hip_INCLUDE_DIRS})
    target_compile_definitions(aurora_runtime PRIVATE AURORA_HIP=1)
    set_property(TARGET aurora_runtime PROPERTY HIP_ARCHITECTURES "gfx908;gfx90a;gfx942")
else()
    message(STATUS "ROCm/HIP not found")
    target_compile_definitions(aurora_runtime PRIVATE AURORA_HIP=0)
endif()

# ═══════════════════════════════════════════════════════════════
# DirectML — GPU matmul for any DX12 GPU (runtime-loaded)
# ═══════════════════════════════════════════════════════════════
if(WIN32)
    target_sources(aurora_runtime PRIVATE aurora/src/runtime/ai/dml_matmul.cpp)
    target_compile_definitions(aurora_runtime PRIVATE AURORA_DML=1)
else()
    target_compile_definitions(aurora_runtime PRIVATE AURORA_DML=0)
endif()

# ═══════════════════════════════════════════════════════════════
# OpenMP — parallel CPU matmul (works on AMD/Intel CPUs)
# ═══════════════════════════════════════════════════════════════
find_package(OpenMP QUIET)
if(OpenMP_CXX_FOUND)
    message(STATUS "Found OpenMP — parallel CPU matmul enabled")
    target_link_libraries(aurora_runtime PRIVATE OpenMP::OpenMP_CXX)
    target_compile_definitions(aurora_runtime PRIVATE AURORA_OPENMP=1)
else()
    message(STATUS "OpenMP not found — building without parallel matmul")
endif()

# ═══════════════════════════════════════════════════════════════
# AVX2/FMA compile flags for collections.cpp (tiled matmul)
# ═══════════════════════════════════════════════════════════════
if(MSVC)
    set_source_files_properties(aurora/src/runtime/std/collections.cpp PROPERTIES COMPILE_FLAGS "/arch:AVX2 /fp:fast")
elseif((CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU") AND CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|amd64|X86|x86|i686")
    set_source_files_properties(aurora/src/runtime/std/collections.cpp PROPERTIES COMPILE_FLAGS "-mavx2 -mfma -ffast-math")
endif()

# ═══════════════════════════════════════════════════════════════
# Optional: MPI support (distributed CPU training)
# ═══════════════════════════════════════════════════════════════
find_package(MPI QUIET)
if(MPI_CXX_FOUND)
    message(STATUS "Found MPI — distributed training enabled")
    target_link_libraries(aurora_runtime PRIVATE MPI::MPI_CXX)
    target_compile_definitions(aurora_runtime PRIVATE AURORA_MPI=1)
else()
    message(STATUS "MPI not found — distributed training will use single-process mode")
endif()

# ═══════════════════════════════════════════════════════════════
# Optional: NCCL support (GPU distributed training)
# ═══════════════════════════════════════════════════════════════
if(CUDAToolkit_FOUND)
    find_path(NCCL_INCLUDE_DIR nccl.h PATHS
        ${CUDAToolkit_INCLUDE_DIRS}
        /usr/include
        /usr/local/include
        /opt/nccl/include
        "C:/Program Files/NVIDIA GPU Computing Toolkit/NCCL"
    )
    find_library(NCCL_LIBRARY nccl
        PATHS ${CUDAToolkit_LIBRARY_DIR} /usr/lib /usr/local/lib /opt/nccl/lib
        "C:/Program Files/NVIDIA GPU Computing Toolkit/NCCL/lib"
    )
    if(NCCL_INCLUDE_DIR AND NCCL_LIBRARY)
        message(STATUS "Found NCCL — GPU distributed training enabled")
        target_include_directories(aurora_runtime PRIVATE ${NCCL_INCLUDE_DIR})
        target_link_libraries(aurora_runtime PRIVATE ${NCCL_LIBRARY})
        target_compile_definitions(aurora_runtime PRIVATE AURORA_NCCL=1)
    else()
        message(STATUS "NCCL not found — GPU distributed training disabled")
    endif()
endif()

# ═══════════════════════════════════════════════════════════════
# Optional: RCCL support (AMD GPU distributed)
# ═══════════════════════════════════════════════════════════════
if(hip_FOUND)
    find_path(RCCL_INCLUDE_DIR rccl/rccl.h
        PATHS /opt/rocm/include /opt/rocm/rccl/include)
    find_library(RCCL_LIBRARY rccl
        PATHS /opt/rocm/lib /opt/rocm/rccl/lib)
    if(RCCL_INCLUDE_DIR AND RCCL_LIBRARY)
        message(STATUS "Found RCCL — AMD GPU distributed training enabled")
        target_include_directories(aurora_runtime PRIVATE ${RCCL_INCLUDE_DIR})
        target_link_libraries(aurora_runtime PRIVATE ${RCCL_LIBRARY})
        target_compile_definitions(aurora_runtime PRIVATE AURORA_RCCL=1)
    else()
        message(STATUS "RCCL not found — AMD GPU distributed training disabled")
    endif()
endif()

# ═══════════════════════════════════════════════════════════════
# Optional: LibTorch (PyTorch C++ API) support
# ═══════════════════════════════════════════════════════════════
find_package(Torch QUIET)
if(Torch_FOUND)
    message(STATUS "Found LibTorch ${Torch_VERSION} — building libtorch_c_api")
    add_library(libtorch_c_api SHARED
        aurora/src/runtime/cpp_interop/libtorch_api.cpp
    )
    target_include_directories(libtorch_c_api PRIVATE
        ${CMAKE_SOURCE_DIR}/aurora/include
        ${TORCH_INCLUDE_DIRS}
    )
    target_link_libraries(libtorch_c_api PRIVATE ${TORCH_LIBRARIES})
    set_property(TARGET libtorch_c_api PROPERTY CXX_STANDARD 17)
    if(MSVC)
        set_property(TARGET libtorch_c_api PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
    endif()
    target_compile_definitions(libtorch_c_api PRIVATE AURORA_LIBTORCH=1)
else()
    message(STATUS "LibTorch not found — skip libtorch_c_api (install PyTorch with C++ support)")
endif()

# ═══════════════════════════════════════════════════════════════
# (test targets removed — test files cleaned up)
# ═══════════════════════════════════════════════════════════════

# ═══════════════════════════════════════════════════════════════
# voss — standalone Aurora Package Manager
# ═══════════════════════════════════════════════════════════════
add_executable(voss
    aurora/tools/voss/voss.cpp
    aurora/tools/voss/io.cpp
    aurora/tools/voss/commands_core.cpp
    aurora/tools/voss/commands_deps.cpp
    aurora/tools/voss/commands_config.cpp
    aurora/tools/voss/commands_template.cpp
    aurora/tools/voss/commands_advanced.cpp
    aurora/tools/voss/commands_theme.cpp
    aurora/tools/voss/commands_bind.cpp
    aurora/tools/voss/bridge_main.cpp
    aurora/tools/voss/bridge_pypi.cpp
    aurora/tools/voss/bridge_npm.cpp
    aurora/tools/voss/bridge_npm_impl.cpp
    aurora/tools/voss/bridge_npm_gen.cpp
    aurora/tools/voss/bridge_cargo.cpp
    aurora/tools/voss/bridge_cargo_impl.cpp
    aurora/tools/voss/bridge_cargo_gen.cpp
    aurora/tools/voss/bridge_native.cpp
    aurora/tools/voss/bridge_jvm.cpp
    aurora/tools/voss/bridge_go.cpp
    aurora/tools/voss/tool_detection.cpp
    aurora/tools/voss/registry_source.cpp
    aurora/src/runtime/interop/type_ir.cpp
    aurora/src/runtime/interop/type_mapping.cpp
    aurora/src/runtime/interop/type_mapping_langs.cpp
)
target_include_directories(voss PRIVATE ${CMAKE_SOURCE_DIR}/aurora/include)
if(WIN32)
    if(MSVC)
        target_link_libraries(voss PRIVATE winhttp.lib ws2_32.lib)
    else()
        target_link_libraries(voss PRIVATE winhttp ws2_32)
    endif()
endif()
if(UNIX AND NOT IOS AND NOT ANDROID)
    find_package(CURL)
    if(CURL_FOUND)
        target_link_libraries(voss PRIVATE CURL::libcurl)
    endif()
endif()

set_target_properties(voss PROPERTIES OUTPUT_NAME "voss")

# ═══════════════════════════════════════════════════════════════
# aurora_registry — standalone Aurora package registry server (Phase 41.1)
# ═══════════════════════════════════════════════════════════════
add_executable(aurora_registry
    aurora/tools/registry/registry_server.cpp
)
if(WIN32)
    target_link_libraries(aurora_registry PRIVATE ws2_32)
else()
    find_package(Threads REQUIRED)
    target_link_libraries(aurora_registry PRIVATE Threads::Threads)
endif()

# ═══════════════════════════════════════════════════════════════
# auroradoc — package documentation generator (Phase 41.4)
# ═══════════════════════════════════════════════════════════════
add_executable(auroradoc
    aurora/tools/auroradoc/auroradoc.cpp
)

# ═══════════════════════════════════════════════════════════════
# Cross-platform bridge build support
# ═══════════════════════════════════════════════════════════════
# Generates a target that compiles a QuickJS npm bridge .c wrapper
# into a shared library (.so / .dylib) for the current platform.
# Usage: add_npm_bridge(<name> <pkg> <c_file>)
function(add_npm_bridge name pkg c_file)
    set(qjs_dir "${CMAKE_SOURCE_DIR}/deps/quickjs")
    set(qjs_sources
        "${qjs_dir}/quickjs.c"
        "${qjs_dir}/cutils.c"
        "${qjs_dir}/libregexp.c"
        "${qjs_dir}/libunicode.c"
        "${qjs_dir}/dtoa.c"
    )
    if(NOT EXISTS "${c_file}")
        message(STATUS "npm bridge '${name}': ${c_file} not found — skipping")
        return()
    endif()
    foreach(qjs_src ${qjs_sources})
        if(NOT EXISTS "${qjs_src}")
            message(STATUS "npm bridge '${name}': missing QuickJS source ${qjs_src} — skipping")
            return()
        endif()
    endforeach()
    add_library(${name} SHARED ${c_file} ${qjs_sources})
    set_target_properties(${name} PROPERTIES LINKER_LANGUAGE C)
    target_include_directories(${name} PRIVATE "${qjs_dir}")
    if(WIN32)
        if(CMAKE_C_COMPILER_ID MATCHES "MSVC")
            set_property(TARGET ${name} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
            target_compile_options(${name} PRIVATE /FI"quickjs_config.h" /std:c11)
        else()
            # MinGW/GCC/Clang: force-include quickjs_config.h too.
            # Without it, CONFIG_VERSION / __JS_INF / PRId64 etc. are missing
            # and quickjs.c fails to compile (error: '__JS_INF' undeclared).
            target_compile_options(${name} PRIVATE -include "${qjs_dir}/quickjs_config.h" -std=c11)
        endif()
        target_link_libraries(${name} PRIVATE bcrypt)
    else()
        target_compile_options(${name} PRIVATE -fPIC)
        if(APPLE)
            set_target_properties(${name} PROPERTIES
                SUFFIX ".dylib"
                INSTALL_RENAME "${CMAKE_SHARED_LIBRARY_PREFIX}${pkg}_npm${CMAKE_SHARED_LIBRARY_SUFFIX}"
            )
        else()
            set_target_properties(${name} PROPERTIES
                OUTPUT_NAME "${pkg}_npm"
            )
            target_link_libraries(${name} PRIVATE pthread dl m)
        endif()
    endif()
    # Rename output to <pkg>_npm.<ext>
    set_target_properties(${name} PROPERTIES
        PREFIX ""
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bridges"
    )
endfunction()

# npm bridges (built only if the generated .c files exist)
add_npm_bridge(bridge_moment     moment     "${CMAKE_SOURCE_DIR}/packages/bridges/npm/moment_npm/moment_npm.c")
add_npm_bridge(bridge_lodash     lodash     "${CMAKE_SOURCE_DIR}/packages/bridges/npm/lodash_npm/lodash_npm.c")
add_npm_bridge(bridge_chalk      chalk      "${CMAKE_SOURCE_DIR}/packages/bridges/npm/chalk_npm/chalk_npm.c")
add_npm_bridge(bridge_execa      execa      "${CMAKE_SOURCE_DIR}/packages/bridges/npm/execa_npm/execa_npm.c")
add_npm_bridge(bridge_got        got        "${CMAKE_SOURCE_DIR}/packages/bridges/npm/got_npm/got_npm.c")
add_npm_bridge(bridge_leftpad    left-pad   "${CMAKE_SOURCE_DIR}/packages/bridges/npm/left-pad_npm/left-pad_npm.c")
add_npm_bridge(bridge_mobx       mobx       "${CMAKE_SOURCE_DIR}/packages/bridges/npm/mobx_npm/mobx_npm.c")
add_npm_bridge(bridge_uuid       uuid       "${CMAKE_SOURCE_DIR}/packages/bridges/npm/uuid_npm/uuid_npm.c")

# ═══════════════════════════════════════════════════════════════
# MySQL native bridge — C helper for MYSQL_ROW field access
# ═══════════════════════════════════════════════════════════════
set(MYSQL_BRIDGE_CPP "${CMAKE_SOURCE_DIR}/packages/bridges/native/mysql_native/mysql_wrapper.cpp")
if(EXISTS "${MYSQL_BRIDGE_CPP}")
    add_library(bridge_mysql_native SHARED "${MYSQL_BRIDGE_CPP}")
    set_target_properties(bridge_mysql_native PROPERTIES
        PREFIX ""
        OUTPUT_NAME "mysql_native"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bridges"
    )
    if(WIN32 AND CMAKE_C_COMPILER_ID MATCHES "MSVC")
        set_property(TARGET bridge_mysql_native PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
    endif()
    message(STATUS "MySQL native bridge: enabled")
else()
    message(STATUS "MySQL native bridge: ${MYSQL_BRIDGE_CPP} not found — skipping")
endif()

# ═══════════════════════════════════════════════════════════════
# aurora_parser — static library (lexer + parser, no LLVM)
# ═══════════════════════════════════════════════════════════════
add_library(aurora_parser STATIC
    aurora/src/compiler/lexer/lexer.cpp
    aurora/src/compiler/lexer/token.cpp
    aurora/src/compiler/parser/parser.cpp
    aurora/src/compiler/parser/parse_stmt.cpp
    aurora/src/compiler/parser/parse_extern.cpp
    aurora/src/compiler/parser/parse_expr.cpp
    aurora/src/compiler/parser/parse_class.cpp
    aurora/src/compiler/parser/parse_struct.cpp
    aurora/src/compiler/parser/parse_union.cpp
    aurora/src/compiler/parser/parse_enum.cpp
    aurora/src/compiler/parser/parse_interface.cpp
    aurora/src/compiler/parser/parse_type_alias.cpp
)
target_include_directories(aurora_parser PUBLIC ${CMAKE_SOURCE_DIR}/aurora/include)

# ═══════════════════════════════════════════════════════════════
# aurora-lsp — Language Server Protocol implementation
# ═══════════════════════════════════════════════════════════════
add_executable(aurora_lsp
    aurora/tools/lsp/lsp.cpp
    aurora/tools/lsp/lsp_json.cpp
    aurora/tools/lsp/lsp_analysis.cpp
    aurora/tools/lsp/lsp_semantic_tokens.cpp
    aurora/tools/lsp/lsp_formatting.cpp
    aurora/tools/lsp/lsp_code_actions.cpp
)
target_link_libraries(aurora_lsp PRIVATE aurora_parser)
target_include_directories(aurora_lsp PRIVATE ${CMAKE_SOURCE_DIR}/aurora/include)

# ═══════════════════════════════════════════════════════════════
# aurora_fmt — source code formatter
# ═══════════════════════════════════════════════════════════════
add_executable(aurora_fmt
    aurora/tools/fmt/fmt.cpp
)
target_link_libraries(aurora_fmt PRIVATE aurora_parser)
target_include_directories(aurora_fmt PRIVATE ${CMAKE_SOURCE_DIR}/aurora/include)

# ═══════════════════════════════════════════════════════════════
# aurora_lint — static analyzer
# ═══════════════════════════════════════════════════════════════
add_executable(aurora_lint
    aurora/tools/lint/lint.cpp
)
target_link_libraries(aurora_lint PRIVATE aurora_parser)
target_include_directories(aurora_lint PRIVATE ${CMAKE_SOURCE_DIR}/aurora/include)

# ═══════════════════════════════════════════════════════════════
# aurora_dap — Debug Adapter Protocol server
# ═══════════════════════════════════════════════════════════════
add_executable(aurora_dap
    aurora/tools/dap/dap.cpp
)
target_include_directories(aurora_dap PRIVATE ${CMAKE_SOURCE_DIR}/aurora/include)

# ═══════════════════════════════════════════════════════════════
# Optional: aurora-bindgen (only if libclang is available)
# ═══════════════════════════════════════════════════════════════
find_package(Clang QUIET)
if(Clang_FOUND)
    message(STATUS "Found Clang — building aurora-bindgen")
    add_executable(aurora_bindgen
        aurora/tools/bindgen/bindgen.cpp
    )
    target_include_directories(aurora_bindgen PRIVATE ${CLANG_INCLUDE_DIRS})
    target_link_libraries(aurora_bindgen PRIVATE libclang)
else()
    message(STATUS "Clang not found — skipping aurora-bindgen")
endif()

# ═══════════════════════════════════════════════════════════════
# Optional: aurora-cppwrap (C++ → extern "C" wrapper generator)
# ═══════════════════════════════════════════════════════════════
if(Clang_FOUND)
    message(STATUS "Found Clang — building aurora-cppwrap")
    add_executable(aurora_cppwrap
        aurora/tools/cppwrap/cppwrap.cpp
    )
    target_include_directories(aurora_cppwrap PRIVATE ${CLANG_INCLUDE_DIRS})
    target_link_libraries(aurora_cppwrap PRIVATE libclang)
endif()

# ═══════════════════════════════════════════════════════════════
# Fiber Engine Tests
# ═══════════════════════════════════════════════════════════════
add_executable(test_fiber
    aurora/tests/runtime/test_fiber.cpp
)
target_link_libraries(test_fiber PRIVATE aurora_runtime)
target_include_directories(test_fiber PRIVATE ${CMAKE_SOURCE_DIR}/aurora/include)
if(ENABLE_ASAN)
    target_compile_options(test_fiber PRIVATE ${ASAN_FLAGS})
    target_link_options(test_fiber PRIVATE ${ASAN_LINK_FLAGS})
endif()
add_test(NAME test_fiber COMMAND test_fiber)

# ═══════════════════════════════════════════════════════════════
# Autograd Engine Tests
# ═══════════════════════════════════════════════════════════════
add_executable(test_autograd
    aurora/tests/runtime/test_autograd.cpp
)
target_link_libraries(test_autograd PRIVATE aurora_runtime)
target_include_directories(test_autograd PRIVATE ${CMAKE_SOURCE_DIR}/aurora/include)
if(ENABLE_ASAN)
    target_compile_options(test_autograd PRIVATE ${ASAN_FLAGS})
    target_link_options(test_autograd PRIVATE ${ASAN_LINK_FLAGS})
endif()
add_test(NAME test_autograd COMMAND test_autograd)

# ═══════════════════════════════════════════════════════════════
# Server Integration Tests (TLS, HTTP, WebSocket, Concurrent)
# ═══════════════════════════════════════════════════════════════
add_executable(test_server
    aurora/tests/runtime/test_server.cpp
)
target_link_libraries(test_server PRIVATE aurora_runtime)
target_include_directories(test_server PRIVATE ${CMAKE_SOURCE_DIR}/aurora/include)
if(WIN32)
    if(MSVC)
        target_link_libraries(test_server PRIVATE ws2_32.lib crypt32.lib)
    else()
        target_link_libraries(test_server PRIVATE ws2_32 crypt32)
    endif()
endif()
if(ENABLE_ASAN)
    target_compile_options(test_server PRIVATE ${ASAN_FLAGS})
    target_link_options(test_server PRIVATE ${ASAN_LINK_FLAGS})
endif()
add_test(NAME test_server COMMAND test_server)

# ═══════════════════════════════════════════════════════════════
# Phase 29: Cross-Platform Validation
# ═══════════════════════════════════════════════════════════════
add_executable(test_crossplatform
    aurora/tests/crossplatform/validate.cpp
)
target_link_libraries(test_crossplatform PRIVATE aurora_runtime)
target_include_directories(test_crossplatform PRIVATE ${CMAKE_SOURCE_DIR}/aurora/include)
if(WIN32)
    if(MSVC)
        target_link_libraries(test_crossplatform PRIVATE ws2_32.lib)
    else()
        target_link_libraries(test_crossplatform PRIVATE ws2_32)
    endif()
endif()
if(ENABLE_ASAN)
    target_compile_options(test_crossplatform PRIVATE ${ASAN_FLAGS})
    target_link_options(test_crossplatform PRIVATE ${ASAN_LINK_FLAGS})
endif()
add_test(NAME test_crossplatform COMMAND test_crossplatform)

# ═══════════════════════════════════════════════════════════════
# Phase 4: AI Benchmark
# ═══════════════════════════════════════════════════════════════
add_executable(bench_ai
    benchmarks/bench_ai.cpp
)
target_link_libraries(bench_ai PRIVATE aurora_runtime)
target_include_directories(bench_ai PRIVATE ${CMAKE_SOURCE_DIR}/aurora/include)

# ═══════════════════════════════════════════════════════════════
# Phase 5: Cross-Ecosystem Bridge Benchmark
# ═══════════════════════════════════════════════════════════════
add_executable(bench_bridge
    benchmarks/bench_bridge.cpp
)
target_include_directories(bench_bridge PRIVATE ${CMAKE_SOURCE_DIR}/aurora/include)

# (test targets from aurora/tests/* removed — test files cleaned up)

# ── Install targets ──
# Mobile cross-compiles (iOS/Android) only build aurora_runtime; the
# desktop CLI targets do not exist there and iOS executables become
# bundles, so skip the install rules on mobile.
if(NOT IOS AND NOT ANDROID)
install(TARGETS aurorac RUNTIME DESTINATION bin)
install(TARGETS aurora_runtime ARCHIVE DESTINATION lib)
install(TARGETS voss RUNTIME DESTINATION bin)
install(TARGETS aurora_registry RUNTIME DESTINATION bin)
install(TARGETS auroradoc RUNTIME DESTINATION bin)
install(TARGETS aurora_lsp RUNTIME DESTINATION bin)
install(TARGETS aurora_fmt RUNTIME DESTINATION bin)
install(TARGETS aurora_lint RUNTIME DESTINATION bin)
install(TARGETS aurora_dap RUNTIME DESTINATION bin)
if(TARGET aurora_bindgen)
    install(TARGETS aurora_bindgen RUNTIME DESTINATION bin)
endif()
if(TARGET aurora_cppwrap)
    install(TARGETS aurora_cppwrap RUNTIME DESTINATION bin)
endif()
endif() # NOT IOS AND NOT ANDROID

