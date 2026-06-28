#pragma once
#include <string>
#include <memory>
#include <vector>
#include <unordered_map>

#include "compiler/ast/ast_memory.hpp"
#include "compiler/ast/ast_stmt.hpp"
#include "compiler/ast/ast_expr.hpp"
#include "compiler/ast/ast_type.hpp"

/* ════════════════════════════════════════════════════════════
   ast.hpp — Unified AST Node Types & Node Structure
   ════════════════════════════════════════════════════════════ */

enum class NodeType {
    /* ── Control Flow ── */
    If, Else, While, For, Loop, Repeat,
    Break, Continue, Skip,
    Match, Case,

    /* ── Function & Class ── */
    Function,
    PerformanceFn,
    Return, Lambda,
    Class, New,

    /* ── Statements ── */
    Assign, IndexAssign,
    Output,
    Delete,

    /* ── Exception ── */
    Try, Throw, Ensure,

    /* ── Async ── */
    Async, Wait, Spawn,

    /* ── Expressions ── */
    BinOp, UnaryOp,
    Var, Num, Float, Str,
    Array, Index,
    Call,
    Attribute,

    /* ── Import ── */
    Import,

    /* ── FFI ── */
    ExternFn,      /* extern ["lib"] function name(params) -> ret */
    ExternStruct,  /* extern struct Name { field: type, ... }      */
    ExternUnion,   /* extern union Name { field: type, ... }       */
    FunctionType,  /* function type signature (for callbacks)      */

    /* ── Misc ── */
    Block, Pass, Yield,

    /* ── Phase 3: Module System ── */
    NamespaceDecl, /* namespace Name: ... */
    ModuleDecl,    /* module name */
    PackageDecl,   /* package name */
    AliasDecl,     /* alias Name = Target */

    /* ── Phase 9: AI/ML & Time/Util ── */
    Ai,           /* AI declaration/scope   */
    Train,        /* model training         */
    Predict,      /* model prediction       */
    Tensor,       /* tensor declaration     */
    Neural,       /* neural layer/network   */
    Sleep,        /* sleep/delay            */
    Time,         /* time access            */
    Random,       /* random value           */

    /* ── Phase 8: Game Engine ── */
    Scene,        /* scene declaration      */
    Entity,       /* entity declaration     */
    Object,       /* game object            */
    Sprite,       /* sprite declaration     */
    Camera,       /* camera declaration     */
    Physics,      /* physics body/world     */
    Collision,    /* collision detection    */
    Audio,        /* audio/sound            */
    Animation,    /* game animation         */
    Input,        /* input handling         */
    Update,       /* update loop            */
    Tick,         /* tick function          */

    /* ── Phase 7: Backend Framework ── */
    Server,       /* server declaration     */
    Request,      /* request object         */
    Response,     /* response object        */
    Api,          /* api route definition   */
    Middleware,   /* middleware pipeline    */
    Database,     /* database connection    */
    Query,        /* database query         */
    Model,        /* data model             */
    Cache,        /* cache declaration      */
    Session,      /* session management     */
    Token,        /* auth token             */
    Auth,         /* authentication         */

    /* ── Phase 6: UI Framework ── */
    Component,    /* component declaration   */
    State,        /* state variable          */
    Properties,   /* properties declaration  */
    Render,       /* render function         */
    Style,        /* style declaration       */
    Theme,        /* theme declaration       */
    Route,        /* route declaration       */
    Page,         /* page declaration        */
    Layout,       /* layout declaration      */
    Animate,      /* animation declaration   */
    Transition,   /* transition declaration  */

    /* ── Phase 5: Async Extensions + Attributes ── */
    Parallel,     /* parallel block       */
    Thread,       /* thread creation      */
    Callback,     /* callback declaration  */
    Event,        /* event declaration     */
    Signal,       /* signal emission       */
    Emit,         /* emit event            */
    Inline,       /* inline hint           */
    NoInline,     /* noinline hint         */
    ConstExpr,    /* compile-time constant */

    /* ── Phase 4: Memory Model Extensions ── */
    Reference,    /* reference x  — create reference */
    Pointer,      /* pointer x    — create raw pointer */
    UnsafeBlock,  /* unsafe: ...  — unsafe block */
    SafeBlock,    /* safe: ...    — safe block */

    /* ── Phase 1: Type System ── */
    StructDecl,     /* struct name: field = val ... */
    StructLiteral,  /* Point { x: 1, y: 2 } — struct value literal */
    EnumDecl,       /* enum name: Variant1, Variant2, ... */
    TypeAlias,      /* type T = int */
    InterfaceDecl,  /* interface Name: method signatures ... */

    /* ── Memory Management ── */
    Move,
    Drop,
    SharedRef,
    WeakRef,
    Borrow,
    Copy,
    Free,

    /* ── Forced Allocation Attributes ── */
    StackAlloc,   /* @stack  — force stack allocation */
    ArenaAlloc,   /* @arena  — force arena allocation */
    RAIIAlloc,    /* @raii   — force RAII allocation */
    ARCAlloc,     /* @arc    — force ARC allocation */
    GCAlloc,      /* @gc     — force GC allocation */

    /* ── Phase 2: Generics / Templates ── */
    TypeParam,    /* T in function foo[T](x: T): T */
    TypeArg,      /* Int in foo[Int](x) call or Point[Int] type */

    /* ── Phase 10: Misc new keywords ── */
    Panic,        /* panic — halt execution */
    Debug,        /* debug — debug output */
    Log,          /* log   — log output */
};

/* ════════════════════════════════════════════════════════════
   ASTNode — the universal AST node
   ════════════════════════════════════════════════════════════ */

struct ASTNode {
    using Ptr = std::unique_ptr<ASTNode>;

    NodeType    type;
    std::string value {};

    Ptr left   {};
    Ptr right  {};
    Ptr body   {};
    Ptr orelse {};
    Ptr next   {};
    Ptr args   {};

    /* Generic / template type parameters (function foo[T, U](...)) and type args (foo[Int, Float](...)) */
    Ptr template_params {};
    Ptr template_args   {};

    int src_line { 0 };

    /* Memory metadata — populated during analysis phases */
    MemoryMetadata memory_meta {};

    /* Type annotation — populated by TypeChecker during semantic analysis.
       Added for H2 staged type-system migration (Phase A). Codegen does
       not fully consume this in Phase A. */
    AstTypeAnnotation type_annotation {};

    /* FFI: varargs flag for extern function (...) */
    bool is_vararg { false };
    /* FFI: calling convention (e.g. "c", "stdcall", "fastcall"). Default = "c" */
    std::string calling_conv { "c" };
    /* FFI cost annotation: "zero", "alloc", "indirection", or "" (unannotated) */
    std::string cost_level {};

    /* Whether this Call node's arguments are consumed (moved) by the callee */
    bool consumes_args { false };
    /* Whether this borrow/node represents a mutable operation */
    bool is_mutable { false };

    /* OOP visibility: "public", "private", "protected", or "" (default public) */
    std::string visibility {};
    /* OOP abstraction flags */
    bool is_abstract { false };
    bool is_final    { false };

    /* Lambda capture variables */
    std::vector<std::string> captures;

    /* NOTE: raw pointer usage (ASTNode*) in this codebase follows an
       owner/observer convention: unique_ptr owns, raw pointer observes.
       The raw pointer patterns (left, right, body, orelse, next, args) are
       intentional — they point to nodes owned by unique_ptrs elsewhere in
       the tree, avoiding circular ownership. */
    explicit ASTNode(NodeType t, std::string v = "", int ln = 0)
        : type(t), value(std::move(v)), src_line(ln) {}

    ASTNode(const ASTNode&)            = delete;
    ASTNode& operator=(const ASTNode&) = delete;
    ASTNode(ASTNode&&)                 = default;
    ASTNode& operator=(ASTNode&&)      = default;
};

inline ASTNode::Ptr make_node(NodeType t, std::string v = "", int ln = 0) {
    return std::make_unique<ASTNode>(t, std::move(v), ln);
}
