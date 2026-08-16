# gen_export_def.cmake
# Reads runtime_exports.hpp and generates a .def file for MinGW
# to export JIT-visible runtime symbols.

if(NOT RUNTIME_EXPORTS_HPP OR NOT GENERATED_DEF)
    message(FATAL_ERROR "RUNTIME_EXPORTS_HPP and GENERATED_DEF must be set")
endif()

file(READ "${RUNTIME_EXPORTS_HPP}" CONTENTS)

# Extract all symbol names from /EXPORT: lines
string(REGEX MATCHALL "/EXPORT:([a-zA-Z0-9_]+)" MATCHED_EXPORTS "${CONTENTS}")

set(GENERATED_CONTENT "; Auto-generated .def file\n")
set(GENERATED_CONTENT "${GENERATED_CONTENT}; Generated from: ${RUNTIME_EXPORTS_HPP}\n")
set(GENERATED_CONTENT "${GENERATED_CONTENT}EXPORTS\n")
# MinGW CRT startup symbol needed by JIT-compiled code
set(GENERATED_CONTENT "${GENERATED_CONTENT}  __main\n")

foreach(MATCH ${MATCHED_EXPORTS})
    string(REGEX REPLACE "/EXPORT:" "" SYM "${MATCH}")
    set(GENERATED_CONTENT "${GENERATED_CONTENT}  ${SYM}\n")
endforeach()

file(WRITE "${GENERATED_DEF}" "${GENERATED_CONTENT}")
list(LENGTH MATCHED_EXPORTS NUM_SYMBOLS)
message(STATUS "Generated ${GENERATED_DEF} with ${NUM_SYMBOLS} exports")
