#pragma once

/* ════════════════════════════════════════════════════════════
   ast_stmt.hpp — Statement Node Types
   ════════════════════════════════════════════════════════════ */

enum class StmtType {
    /* control flow */
    If,
    Else,
    While,
    For,
    Loop,
    Break,
    Continue,
    Skip,
    Match,
    Case,

    /* function & class */
    Function,
    PerformanceFn,
    Return,
    Lambda,
    Class,
    New,

    /* statements */
    Assign,
    IndexAssign,
    Output,
    Delete,

    /* exception */
    Try,
    Throw,
    Ensure,

    /* async */
    Async,
    Wait,
    Spawn,

    /* memory management */
    Move,
    Drop,
    SharedRef,
    WeakRef,
    Borrow,
    Copy,
    Free,

    /* import */
    Import,

    /* misc */
    Block,
    Pass,
};
