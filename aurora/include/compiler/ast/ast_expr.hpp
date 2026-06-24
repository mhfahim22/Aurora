#pragma once

/* ════════════════════════════════════════════════════════════
   ast_expr.hpp — Expression Node Types
   ════════════════════════════════════════════════════════════ */

enum class ExprType {
    /* arithmetic & logic */
    BinOp,
    UnaryOp,

    /* literals & variables */
    Var,
    Num,
    Float,
    Str,

    /* collections */
    Array,
    Index,

    /* function call */
    Call,

    /* attribute access */
    Attribute,
};
