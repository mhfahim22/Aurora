# gen_linker_includes.cmake
# Reads runtime_exports.hpp and generates linker_include_symbols.cpp
# For MSVC: #pragma comment(linker, "/include:...") for each exported symbol.
# For MinGW: extern "C" __declspec(dllexport) declarations with reference array.
# This prevents the linker from stripping JIT-visible runtime symbols.

if(NOT RUNTIME_EXPORTS_HPP OR NOT GENERATED_CPP)
    message(FATAL_ERROR "RUNTIME_EXPORTS_HPP and GENERATED_CPP must be set")
endif()

file(READ "${RUNTIME_EXPORTS_HPP}" CONTENTS)

# Extract all symbol names from /EXPORT: lines
string(REGEX MATCHALL "/EXPORT:([a-zA-Z0-9_]+)" MATCHED_EXPORTS "${CONTENTS}")

# Windows-only backend symbols (ui_win32.cpp) are not compiled on other
# platforms, so they must not be force-referenced by the generated array.
if(NOT WIN32)
    list(FILTER MATCHED_EXPORTS EXCLUDE REGEX "^/EXPORT:aurora_ui_win32_")
endif()

set(GENERATED_CONTENT "// Auto-generated linker include directives\n")
set(GENERATED_CONTENT "${GENERATED_CONTENT}// Generated from: ${RUNTIME_EXPORTS_HPP}\n\n")

# MSVC #pragma section
set(GENERATED_CONTENT "${GENERATED_CONTENT}#ifdef _MSC_VER\n")
set(GENERATED_CONTENT "${GENERATED_CONTENT}#pragma comment(linker, \"/DEFAULTLIB:aurora_runtime.lib\")\n")

foreach(MATCH ${MATCHED_EXPORTS})
    string(REGEX REPLACE "/EXPORT:" "" SYM "${MATCH}")
    set(GENERATED_CONTENT "${GENERATED_CONTENT}#pragma comment(linker, \"/include:${SYM}\")\n")
endforeach()

set(GENERATED_CONTENT "${GENERATED_CONTENT}#endif // _MSC_VER\n\n")

# MinGW/other: extern "C" declarations + reference array to force symbol
# inclusion from the static library. Exports are handled by .def file.
set(GENERATED_CONTENT "${GENERATED_CONTENT}#ifndef _MSC_VER\n")
set(GENERATED_CONTENT "${GENERATED_CONTENT}// Force JIT-visible symbols into the executable image\n")

foreach(MATCH ${MATCHED_EXPORTS})
    string(REGEX REPLACE "/EXPORT:" "" SYM "${MATCH}")
    set(GENERATED_CONTENT "${GENERATED_CONTENT}extern \"C\" void ${SYM}();\n")
endforeach()

set(GENERATED_CONTENT "${GENERATED_CONTENT}\n")
# used+retain keeps the reference array alive under -Wl,--gc-sections so the
# runtime archive members defining the exported symbols are actually pulled in.
set(GENERATED_CONTENT "${GENERATED_CONTENT}#if defined(__GNUC__) || defined(__clang__)\n")
set(GENERATED_CONTENT "${GENERATED_CONTENT}__attribute__((used, retain))\n")
set(GENERATED_CONTENT "${GENERATED_CONTENT}#endif\n")
set(GENERATED_CONTENT "${GENERATED_CONTENT}static void* _aurora_export_refs[] = {\n")

foreach(MATCH ${MATCHED_EXPORTS})
    string(REGEX REPLACE "/EXPORT:" "" SYM "${MATCH}")
    set(GENERATED_CONTENT "${GENERATED_CONTENT}    (void*)&${SYM},\n")
endforeach()

set(GENERATED_CONTENT "${GENERATED_CONTENT}};\n")
set(GENERATED_CONTENT "${GENERATED_CONTENT}#endif // !_MSC_VER\n")

file(WRITE "${GENERATED_CPP}" "${GENERATED_CONTENT}")
list(LENGTH MATCHED_EXPORTS NUM_SYMBOLS)
message(STATUS "Generated ${GENERATED_CPP} with ${NUM_SYMBOLS} symbols")
