#pragma once
#include <cstdint>
#include <cstddef>

/* ════════════════════════════════════════════════════════════
    types.hpp — Common Type Aliases
    ════════════════════════════════════════════════════════════
    Fixed-size integer types and commonly used type aliases
    shared across the Aurora compiler and runtime.
    ════════════════════════════════════════════════════════════ */

namespace aurora {

/* ── Fixed-width integers ── */
using i8  = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;

using u8  = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

using f32 = float;
using f64 = double;

using usize = std::size_t;
using isize = std::ptrdiff_t;

/* ── Byte ── */
using byte = unsigned char;

} // namespace aurora

/* ── Global aliases for convenience ── */
using aurora::i8;
using aurora::i16;
using aurora::i32;
using aurora::i64;
using aurora::u8;
using aurora::u16;
using aurora::u32;
using aurora::u64;
using aurora::f32;
using aurora::f64;
using aurora::usize;
using aurora::isize;
using aurora::byte;
