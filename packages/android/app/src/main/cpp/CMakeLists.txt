# ════════════════════════════════════════════════════════════
# Aurora Android NDK — CMakeLists.txt
# ════════════════════════════════════════════════════════════

cmake_minimum_required(VERSION 3.22)
project(aurora_app C CXX)

set(AURORA_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../../../../aurora")
set(THIRD_PARTY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../../../../third_party")

# ── Compile the Aurora runtime sources ──

# Core runtime
file(GLOB_RECURSE AURORA_RUNTIME_SOURCES
    "${AURORA_SOURCE_DIR}/src/runtime/*.cpp"
    "${AURORA_SOURCE_DIR}/src/runtime/*.c"
    "${AURORA_SOURCE_DIR}/src/runtime/*.asm"
)

# Standard library modules
file(GLOB AURORA_STD_SOURCES
    "${AURORA_SOURCE_DIR}/src/std/*.cpp"
)

# Mobile runtime
file(GLOB_RECURSE AURORA_MOBILE_SOURCES
    "${AURORA_SOURCE_DIR}/src/mobile/android/*.cpp"
)

# SQLite3
set(SQLITE3_SOURCE "${THIRD_PARTY_DIR}/sqlite3/sqlite3.c")

# ── JNI bridge entry point ──
add_library(aurora_app SHARED
    jni_bridge.cpp
    ${AURORA_RUNTIME_SOURCES}
    ${AURORA_STD_SOURCES}
    ${AURORA_MOBILE_SOURCES}
    ${SQLITE3_SOURCE}
)

target_include_directories(aurora_app PRIVATE
    ${AURORA_SOURCE_DIR}/include
    ${AURORA_SOURCE_DIR}/third_party
    ${THIRD_PARTY_DIR}
)

target_compile_options(aurora_app PRIVATE
    -std=c++20
    -fexceptions
    -frtti
    -Wall
    -Wextra
)

target_link_libraries(aurora_app
    android
    EGL
    GLESv2
    log
    z
)
