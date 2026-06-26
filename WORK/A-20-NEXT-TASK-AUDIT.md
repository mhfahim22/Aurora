# TASK 20 — Next Task Selection: Maximum Audit Mode

**Role:** Patch Auditor & Architecture Reviewer  
**Date:** 2026-06-26  
**Phase:** Post-stabilization / H2 Complete  

---

## A) Repository Verification

### H2 Phase Status

| Phase | Scope | Status | Evidence |
|-------|-------|--------|----------|
| **H2-A** | AST annotation schema (`ast_type.hpp`, `type_annotation` on `ASTNode`) | ✅ **COMPLETE** | File exists at `include/compiler/ast/ast_type.hpp` — 21-variant `AstTypeKind`, `AstTypeAnnotation` struct. `ASTNode::type_annotation` field at `include/compiler/ast.hpp:186`. |
| **H2-B** | Annotation write sites (`annotate_node`, `annotate_ret`, `to_ast_type_kind`) | ✅ **COMPLETE** | 58 write sites across `typechecker.cpp` confirmed via `Select-String`. Helper functions at lines 51, 67, 122, 686, 717, 798, 851, 856, 959, 977, 1011, 1061, 1116, 1161, 1180, 1189, 1212, 1214. |
| **H2-C** | Codegen consumption (`get_annotation_kind`, 5 codegen files) | ✅ **COMPLETE** | `get_annotation_kind()` at `codegen.hpp:665`. Consumers: `codegen_stmt.cpp:85/279/370/690`, `codegen_ffi.cpp:83/111`, `codegen_expr.cpp`, `codegen_function.cpp`, `codegen_builtins_section1.cpp`. |
| **H2-D** | Function/Lambda/ExternFn declaration annotations | ✅ **COMPLETE** | Function result type annotated (line 1161), Lambda (1189), ExternFn params + return, PerformanceFn. `gen_extern_fn` annotation-first (`codegen_ffi.cpp:83/111`). |
| **H2-D2** | Complete coverage + OOP annotation migration | ✅ **COMPLETE** | Struct/Enum/Interface/TypeAlias (`typechecker_types.cpp:120/147/185/45`), Class (`typechecker.cpp:798`), For-index (1061), Match vars (122), Delete/Move/etc. (1212/1214), Async (851/856). `ClassFieldInfo.type_kind` + `ClassMethodInfo.return_kind` in `class_oop.hpp:51/64`. 4 helpers in `codegen_oop.cpp:114-143`. 7 OOP functions wired. |

### Phase Design Status

```
Phase 1 (Critical patching):   Tasks 1-3  → ✅ ALL DONE
Phase 2 (Runtime safety):      Tasks 4-5   → ✅ ALL DONE
Phase 3 (Portability):         Tasks 6-10  → ✅ ALL DONE
Phase 4 (Architecture audit):  Tasks 11-13 → ✅ ALL DONE (C4/H7/H2)
Phase 5 (Compiler quality):    Tasks 14-16 → ✅ ALL DONE (M1/M5/M7/M8)
Phase 6 (Maintenance loop):    Tasks 17-20 → ✅ In progress (Task 20 now)
```

All 20 tasks from `WORK/phase_design.txt` are either completed or in their final step.

---

## B) Remaining Roadmap — Full Inventory

### All Execution Board Tasks by Status

#### ✅ DONE (19 tasks)
A-01, A-02, A-03, A-04, A-05, A-06, A-07, A-08, A-09, A-10, A-11, A-12, A-13, A-14, A-15, A-16, A-17, A-18, A-PRE1, A-PRE2, A-PRE3a, A-PRE4, A-PRE5

#### ⏳ DEFERRED (3 tasks)

| ID | Item | Priority | Area | Description | Blocking? |
|----|------|----------|------|-------------|-----------|
| A-PRE3b | PRE-3b | P2 | Testing | `test_memory_safety` can't link standalone; needs compiler static lib CMake target | Needs CMake infra change |
| A-PRE6 | PRE-6 | P3 | Portability | Bare `#elif __APPLE__` without `defined()` — 7 instances, works by accident | None |
| A-PRE7 | PRE-7 | P3 | Portability | Dead i386 macro in platform detection — may already be removed | None |

#### 🆕 UNSTARTED — Report Items Not on Board

The original Aurora report lists 10 Medium-severity items (M1-M10). Two were captured as separate board items (M1→A-15, M5→A-16), and M7/M8→A-17/A-18. The remainder were never assigned board IDs or addressed:

| Report ID | Description | Priority | Category | Effort | Risk |
|-----------|-------------|----------|----------|--------|------|
| **M2** | `source_path.find_last_of` called before empty-string check (main.cpp:976,1220) | P3 | Bug fix | ~1 edit site | Very low |
| **M3** | Non-Windows GUI functions return nullptr (gui.cpp:625-644) | P3 | Portability | ~5 edit sites | Low |
| **M4** | Use-after-move: `module` moved then `setSourceFileName` called (main.cpp:1037) | P2 | Bug fix | ~1 edit site | Low |
| **M6** | Namespace/module scoping unimplemented (main.cpp:419 TODO) | P3 | Feature | Very large (100+ sites) | High |
| **M9** | Many NodeTypes (Server, Api, Database, Route, Page) are pass-through stubs (typechecker.cpp:733-822) | P2 | Bug fix / Type system | ~10-20 edit sites | Medium |
| **M10** | 762+ printf/cout/cerr calls throughout production code | P4 | Maintenance | Very large (entire codebase) | Very low |

#### ⏳ H2 Phase E — Deferred from H2 Completion

| Item | Priority | Risk | Description | Effort |
|------|----------|------|-------------|--------|
| **E-1** | High | Low | Element type propagation (`element_kind` for arrays/lists/maps) | ~20 edit sites |
| **E-2** | High | Low | Field-level annotations (schema extension for per-field AstTypeKind) | ~15 edit sites |
| **E-3** | Medium | Medium | `gen_function()` LLVM type selection from annotations | ~10 edit sites (risky ABI change) |
| **E-4** | Low | High | Generic/template type tracking | Very large (requires design) |

#### ⏳ Backend Migration — Deferred

| Item | Priority | Description | Effort |
|------|----------|-------------|--------|
| **B-1** | Medium | `optimized_codegen.cpp` annotation migration | ~20 edit sites |
| **B-2** | Medium | `ast_to_ir.cpp` annotation migration | ~15 edit sites |

---

## C) Candidate Comparison

### Selection Criteria
1. **Architectural ROI** — Does this improve the compiler's fundamental capabilities?
2. **Regression risk** — Can this be done safely without breaking existing tests?
3. **Phase ordering** — Does this follow naturally from completed work?
4. **Not blocked** — No dependencies on unfinished work
5. **Roadmap consistency** — Aligned with Aurora's stated direction (static type system)

### Candidate Pool

| # | Candidate | Category | ROI | Risk | Effort | Blocked? | Phase Match |
|---|-----------|----------|-----|------|--------|----------|-------------|
| 1 | **H2 Phase E-1: Element kind propagation** | Feature (metadata) | ★★★★★ | Very Low | Medium (20 sites) | No | ✅ Direct H2 continuation |
| 2 | **H2 Phase E-2: Field-level annotations** | Feature (metadata) | ★★★★ | Low | Medium (15 sites) | No | ✅ Direct H2 continuation |
| 3 | **H2 Phase E-3: gen_function() LLVM ABI** | Feature (codegen) | ★★★ | Medium | Medium (10 sites) | Needs E-1 | ❌ Blocked by E-1/2 |
| 4 | **M4: Use-after-move (main.cpp)** | Bug fix | ★ | Very Low | Trivial (1 site) | No | ⚠️ Tangent |
| 5 | **M2: find_last_of empty check** | Bug fix | ★ | Very Low | Trivial (1 site) | No | ⚠️ Tangent |
| 6 | **M9: Pass-through NodeTypes** | Bug fix | ★★★ | Medium | Medium (15 sites) | No | ⚠️ Partial overlap with H2 |
| 7 | **M6: Namespace/module scoping** | Feature | ★★ | High | Very large | No | ❌ Premature |
| 8 | **M8: CI pipeline implementation** | Infrastructure | ★★★★ | Low | Large | No | ⚠️ Phase 5 follow-up |
| 9 | **M3: Non-Windows GUI nullptr** | Portability | ★ | Low | Small (5 sites) | No | ❌ Low priority P3 |
| 10 | **A-PRE3b: test_memory_safety linkage** | Build fix | ★★ | Low | Medium | No | ⚠️ Low architectural value |
| 11 | **B-1: optimized_codegen migration** | Maintenance | ★★★ | Low | Medium (20 sites) | No | ⚠️ Separate backend |

### Candidate Elimination

- **M4, M2** → Quick bugs, low architectural ROI. Can be done anytime. Not the highest-value next task.
- **M9** → Partially addressed by H2. The Server/Api/Database/Route/Page node types are pass-through stubs, but many relate to runtime types, not compiler IR types. Would benefit from H2 Phase E first.
- **M6** → Feature-scale effort (100+ sites). Premature without a stable type system foundation. This is a post-H2-F task.
- **M8** → High infrastructure value, but doesn't improve the compiler correctness or capabilities. Should be done as a parallel activity, not the primary next task.
- **M3** → P3 portability. Low value for the current stage.
- **A-PRE3b** → Deferred for good reason (needs CMake static lib target). Build system change with no architectural impact.
- **B-1/B-2** → Alternate backends. Worth doing but H2 Phase E has higher ROI.

### Winner Analysis: H2 Phase E-1 (Element Kind Propagation)

**Why H2 Phase E-1 beats all other candidates:**

1. **Leverages warm context**: The H2 pipeline is fresh. `AstTypeAnnotation.element_kind` is already a field on every node — but it's always set to `Unknown`. Filling it requires only TypeChecker-side changes; the codegen consumers are already wired.

2. **Largest remaining annotation gap**: Every array/list/map/set/vector/stack/queue literal, variable, and function return carries `element_kind::Unknown`. This means codegen treats all arrays as opaque — it can't specialize per-element codegen.

3. **Directly unblocks downstream work**: Element kind propagation is the prerequisite for:
   - Specialized array codegen (string arrays → i8** instead of i64*)
   - Better type checking for array operations
   - Generic array abstractions
   - Phase E-2 (field-level annotations) becomes easier with the element tracking pattern established

4. **Lowest regression risk in the pool**: Element kind is a pure additive field. No consumer currently reads it (it's checked nowhere). Setting it from Unknown to a real value changes nothing until a consumer is wired to read it. Phase E-1 is the safest large change possible.

5. **Matches the original H2 plan exactly**: The H2D2 document explicitly identifies element kind propagation as the highest-priority Phase E work.

**Why E-1 before E-2**: Element kind propagation is simpler (single field, trace through existing type inference) than field-level annotations (requires schema extension, new data structures). Doing E-1 first establishes the pattern for per-element metadata without requiring schema changes.

---

## D) Selected Task

### Recommended Next Task: **H2 Phase E-1 — Element Kind Propagation**

Populate `AstTypeAnnotation::element_kind` with resolved element types for all compound type annotations (arrays, lists, maps, sets, vectors, stacks, queues).

| Property | Value |
|----------|-------|
| **Board ID** | A-19 (proposed) |
| **Task Pack ID** | Task 21 (proposed) |
| **Report Item** | H2 follow-up |
| **Priority** | P1 |
| **Category** | Feature (type system metadata enrichment) |
| **Type** | Patch + Audit |
| **Architectural ROI** | ★★★★★ |
| **Regression Risk** | Very Low |
| **Estimated Effort** | ~20 edit sites across 3-4 files |
| **Blocking Dependencies** | None (H2 A-D2 complete) |
| **Blocks** | H2 Phase E-2, E-3, generic arrays, optimized array codegen |

### Runner-Up

**H2 Phase E-2 — Field-Level Annotations**  
Why not selected first: Requires schema extension (new data structures, possibly `std::vector<AstTypeAnnotation>` per node or a separate annotation map). E-1 is simpler and should be done first to establish the pattern.

---

## E) Deep Technical Audit

### 5.1 Current Problem

`AstTypeAnnotation::element_kind` (`ast_type.hpp:74`) is declared as a field on every AST node:

```cpp
AstTypeKind element_kind { AstTypeKind::Unknown };  // For arrays: element type
```

In all 58+ `annotate_node()` call sites across the TypeChecker, `element_kind` is never set — it remains `AstTypeKind::Unknown` in every case. This means:

- **Array literals** (`[1, 2, 3]`, `["a", "b"]`) carry `kind=Array, element_kind=Unknown`
- **List/Map/Set/Vector/Stack/Queue literals** carry their type kind but element_kind=Unknown
- **Variables bound to arrays/lists** carry Array/List kind but no element type info
- **Function returns that are arrays** carry Array kind but no element type info
- **For-loop iteration over arrays** (`for x in arr`) detects Array kind but doesn't know the element type
- **Array element access** (`arr[i]`) cannot statically determine the element type

### 5.2 Root Cause

The `annotate_node()` helper function (`typechecker.cpp:51`) only sets `kind` and `type_name`:

```cpp
static void annotate_node(const ASTNode* node, AuroraType type,
                           const std::string& user_type_name = "") {
    auto* mut = const_cast<ASTNode*>(node);
    mut->type_annotation.kind = to_ast_type_kind(type);
    mut->type_annotation.type_name = user_type_name;
    // element_kind is NEVER set here
}
```

No overload or additional parameter exists to carry the element type. When the TypeChecker infers the type of an array literal (`infer_array_literal()`), it determines the element type from the first element but this information is not propagated to the annotation.

Similarly, `infer_expr()` and `walk_stmt()` set the node's overall type but not the element type for compound types.

### 5.3 Affected Subsystem

**Primary:** TypeChecker (annotation write sites)  
**Secondary:** Codegen (annotation consumers — no changes needed in Phase E-1, but readiness for future consumption)

### 5.4 Existing Implementation

The element type is already **internally available** at several TypeChecker locations but discarded:

1. **Array literal inference** (`infer_array_literal()` in `typechecker.cpp`): Determines element type from first element, stores it in `TypeInfo` (Aurora's internal type representation), but this is not mapped to `AstTypeAnnotation.element_kind`

2. **`infer_expr()` return**: Returns `AuroraType` (scalar enum) — doesn't carry element type information

3. **`walk_stmt()` variable binding**: Annotates the node with the resolved type but not the element type

4. **Function return type inference**: The `FunctionTypeInfo.result` is a single `AuroraType` — doesn't include element type

The `AuroraType` enum (`typechecker.hpp`) has entries like `Array`, `List`, `Map`, etc., but these are atomic enum values — there's no notion of `Array(Int)` vs `Array(String)`. The element type is inferred during analysis (e.g., from the first element of an array literal) but not preserved in the type annotation.

### 5.5 Files Involved

| File | Role | Current state | Changes needed |
|------|------|---------------|----------------|
| `include/compiler/ast/ast_type.hpp` | AstTypeAnnotation definition | `element_kind` field exists, default Unknown | No structural change |
| `include/compiler/typechecker.hpp` | AuroraType enum + FunctionTypeInfo | No element type carrier on FunctionTypeInfo | Minor: add element field or leave unchanged |
| `src/compiler/semantic/typechecker.cpp` | Main write sites | ~58 annotate_node calls, all skip element_kind | ~15-20 need element_kind population |
| `src/compiler/semantic/typechecker_types.cpp` | Type registration | annotate_node_decl calls | Minor: populate from resolved type |
| `src/compiler/semantic/typechecker_oop.cpp` | OOP registration | ClassFieldInfo type_kind set | Add element_kind field to ClassFieldInfo? (Phase E-2 scope) |

### 5.6 Functions Involved

| Function | File | Current behaviour | Phase E-1 change |
|----------|------|-------------------|------------------|
| `annotate_node()` | `typechecker.cpp:51` | Sets kind + type_name | Add element_type parameter (default Unknown) |
| `infer_array_literal()` | `typechecker.cpp` | Determines element type, returns AuroraType::Array | Pass element type to annotate_node |
| `infer_expr()` | `typechecker.cpp` | Returns AuroraType scalar | Needs element type carrier in return signature |
| `walk_stmt()` (Assign/Var) | `typechecker.cpp` | Annotates LHS with RHS type | Pass element type from RHS |
| `walk_stmt()` (For) | `typechecker.cpp` | Annotates loop var as Int | Could infer element type from range expr |
| `walk_stmt()` (Return) | `typechecker.cpp` | Annotates return value | Pass element type from returned expr |
| `annotate_node_decl()` | `typechecker_types.cpp:9` | Sets kind + type_name | Add element_type parameter |
| `register_type_alias()` | `typechecker_types.cpp` | Resolves base type | Propagate element type for compound aliases |

### 5.7 Data Flow

```
Parser                          TypeChecker                          Codegen
──────                          ──────────                          ───────
ArrayLiteral ──▶ infer_array_literal() ──▶ annotate_node(node, Array, "")
  first_elem                          │       element_kind = Unknown ← BUG
  └── Int                             │
                                      ▼
                                    ASTNode
                                    type_annotation.kind = Array
                                    type_annotation.element_kind = Unknown

After Phase E-1:
ArrayLiteral ──▶ infer_array_literal() ──▶ annotate_node(node, Array, "", Int)
  first_elem                          │       element_kind = Int ← FIXED
  └── Int                             │
                                      ▼
                                    ASTNode
                                    type_annotation.kind = Array
                                    type_annotation.element_kind = Int
```

### 5.8 Dependency Graph

```
H2 Phase A ──▶ H2 Phase B ──▶ H2 Phase C ──▶ H2 Phase D ──▶ H2 Phase D2
    │               │               │               │               │
    ▼               ▼               ▼               ▼               ▼
  ast_type       annotate_      get_annotation_   Function/     Complete
  .hpp           node() sites   kind() consumers  Extern       coverage
  ───────        ───────────    ────────────────  ─────────    ─────────
  
                                                      │
                                                      ▼
                                              ┌─────────────────┐
                                              │ H2 Phase E-1 ◀──┼── YOU ARE HERE
                                              │ Element Kind    │
                                              │ Propagation     │
                                              └────────┬────────┘
                                                       │
                                                       ▼
                                              ┌─────────────────┐
                                              │ H2 Phase E-2    │
                                              │ Field-level     │
                                              │ Annotations     │
                                              └────────┬────────┘
                                                       │
                                                       ▼
                                              ┌─────────────────┐
                                              │ H2 Phase E-3    │
                                              │ gen_function()  │
                                              │ LLVM ABI        │
                                              └─────────────────┘
```

### 5.9 Estimated Edit Sites

- `typechecker.cpp` `annotate_node()` signature + body: **2 edits**
- `typechecker.cpp` `annotate_ret()` signature + body: **2 edits**
- `typechecker.cpp` `infer_array_literal()`: **1-2 edits** (pass element type)
- `typechecker.cpp` `infer_expr()`: **2-3 edits** (carry element type, may need struct return or out parameter)
- `typechecker.cpp` `walk_stmt()` (Assign): **3-5 edits** (propagate element type from RHS to LHS)
- `typechecker.cpp` `walk_stmt()` (For): **1 edit** (element type from range expr)
- `typechecker.cpp` `walk_stmt()` (Return): **2 edits** (propagate from returned expr)
- `typechecker.cpp` `walk_stmt()` (Delete/Move/Drop/etc.): **3-5 edits** (propagate from lookup_var)
- `typechecker_types.cpp` `annotate_node_decl()`: **2 edits** (signature + body)
- `typechecker_types.cpp` `register_type_alias()`: **1 edit** (element type for compound aliases)

**Total: ~20-25 edit sites across 2-3 files**

### 5.10 Estimated LOC

- ~50-80 lines of new/changed code (signature changes, element type extraction, annotation propagation)
- ~0 lines removed
- ~0 lines in new files

### 5.11 Testing Impact

- **No behavioural change** — element_kind is read by zero consumers in the current codebase
- All 16 existing tests should continue to pass identically (no codegen output changes)
- No new tests strictly required for Phase E-1 (pure metadata enrichment)
- **Recommended**: Add verification tests that check `element_kind` is correctly set for array literal cases

### 5.12 Regression Risk

**VERY LOW.** Element kind is an additive metadata field. It is checked by zero codegen consumers:
- `get_annotation_kind()` reads only `type_annotation.kind` — not element_kind
- `codegen_oop.cpp` helpers check `type_kind`/`return_kind` — not element_kind
- `gen_for()` checks `get_annotation_kind()` — which returns `kind`, not element_kind
- `gen_expr()` uses `expr_is_string_type()` — checks `kind`, not element_kind
- No code in the codebase reads `element_kind`

Setting it from Unknown to a real value has precisely zero observable effect until Phase E-2 or E-3 introduces a consumer.

### 5.13 Migration Strategy

**Incremental per-expression-type approach:**

1. **Phase E-1a**: Add `element_type` parameter to `annotate_node()` with default `AuroraType::Unknown` (backwards compatible with all 58 existing call sites)
2. **Phase E-1b**: Wire `infer_array_literal()` to pass resolved element type
3. **Phase E-1c**: Wire `infer_expr()` element type propagation (may need structural change to return type)
4. **Phase E-1d**: Wire assignment/return element type propagation
5. **Phase E-1e**: Wire memory operations (Delete/Move/etc.) element type propagation

Each substep compiles and tests independently.

### 5.14 Rollback Strategy

**Trivial.** `element_kind` is an isolated field on `AstTypeAnnotation`:
- To roll back: revert the `annotate_node()` signature change (setting element_kind to Unknown)
- All 58 existing call sites continue to work (element_kind defaults to Unknown)
- No downstream code is affected
- Rollback commit: single `git revert` of the Phase E-1 commit

---

## F) Phase Breakdown

H2 Phase E-1 can be split into 5 independently-compilable sub-phases:

### Phase E-1a: annotate_node() Element Type Parameter

**Scope:** Modify `annotate_node()` and `annotate_ret()` to accept an optional `AuroraType element_type` parameter (default `AuroraType::Unknown`). Set `element_kind` from this parameter.

**Files:** `typechecker.cpp` (2 functions)  
**Edit sites:** 4  
**Compiles independently?** ✅ Yes — no call sites change yet, all use default  
**Behaviour change:** None (element_kind remains Unknown at all call sites)  
**Test impact:** None  

### Phase E-1b: Array Literal Element Type

**Scope:** In `infer_array_literal()`, extract the element type from the first element's inferred type and pass it to `annotate_node()`.

**Files:** `typechecker.cpp` (1 function)  
**Edit sites:** 1-2  
**Compiles independently?** ✅ Yes (depends on E-1a)  
**Behaviour change:** None (element_kind set to inferred type, but no consumer reads it)  
**Test impact:** None observable, but can add verification tests  

### Phase E-1c: infer_expr() Element Type Carrier

**Scope:** `infer_expr()` currently returns a single `AuroraType`. To carry element type, change to a small struct return (`InferredType { AuroraType kind; AuroraType element_kind; }`) or add an out parameter.

**Files:** `typechecker.cpp` (infer_expr + all callers)  
**Edit sites:** 8-12 (infer_expr definition + all ~10 callers)  
**Compiles independently?** ✅ Yes (depends on E-1a)  
**Behaviour change:** None (element_kind populated but not consumed)  
**Risk:** Moderate — changing `infer_expr()` return type requires updating all callers. Use `auto [kind, elem]` structured bindings for clean migration.

### Phase E-1d: Assignment/Return Element Propagation

**Scope:** In `walk_stmt()`, propagate element_kind from RHS to LHS in assignments, and from returned expressions to return value annotations.

**Files:** `typechecker.cpp` (walk_stmt Assign/Var/Return handlers)  
**Edit sites:** 5-7  
**Compiles independently?** ✅ Yes (depends on E-1c)  
**Behaviour change:** None  
**Test impact:** None  

### Phase E-1e: For-loop + Memory Ops Element Propagation

**Scope:** In `walk_stmt()`, set `element_kind` on For-loop range expressions (if the range is an array/collection) and on Delete/Move/Drop/etc. nodes from the looked-up variable's element type.

**Files:** `typechecker.cpp` (walk_stmt For/Delete/Move/Drop/Borrow/Copy/Free/Reference/Pointer handlers)  
**Edit sites:** 5-8  
**Compiles independently?** ✅ Yes (depends on E-1c)  
**Behaviour change:** None  
**Test impact:** None  

---

## G) Implementation Roadmap

### Step-by-Step Execution Plan

```
STEP 1: Phase E-1a — annotate_node() signature
  ─────────────────────────────────────────────
  File:  typechecker.cpp
  Change:
    - Add AuroraType element_type = AuroraType::Unknown parameter to annotate_node()
    - Add AuroraType element_type = AuroraType::Unknown parameter to annotate_ret()
    - Set mut->type_annotation.element_kind = to_ast_type_kind(element_type) in body
  Build:  aurorac (Release) → 0 errors
  Test:   All 16 tests → all pass (identical behaviour)
  Review: Check that default parameter preserves all 58 call sites unchanged

STEP 2: Phase E-1b — Array literal element type
  ──────────────────────────────────────────────
  File:  typechecker.cpp
  Change:
    - In infer_array_literal(), extract element type from first element:
      AuroraType elem_type = infer_expr(first_elem);
      (or use existing resolved type from internal TypeInfo)
    - Pass elem_type to annotate_node()
  Build:  aurorac (Release) → 0 errors
  Test:   All 16 tests → all pass
  Audit:  Verify with array literals of different element types (int, float, string)

STEP 3: Phase E-1c — infer_expr() element type carrier
  ─────────────────────────────────────────────────
  File:  typechecker.cpp + typechecker.hpp (if struct needed)
  Option A: Out parameter (simpler, less intrusive)
    void infer_expr(const ASTNode* node, AuroraType& out_kind, AuroraType& out_elem);
    // Keeps existing callers working with minimal edits
  Option B: Return struct
    struct InferredType { AuroraType kind; AuroraType elem; };
    InferredType infer_expr(const ASTNode* node);
    // Cleaner but requires updating all ~10 callers
  Build:  aurorac (Release) → 0 errors
  Test:   All 16 tests → all pass
  Review: Confirm all callers updated correctly

STEP 4: Phase E-1d — Assignment/Return propagation
  ─────────────────────────────────────────────
  Files: typechecker.cpp
  Change:
    - Assign: after LHS type is set, propagate element_kind from RHS
    - Return: after annotating return value, propagate element_kind
  Build:  aurorac (Release) → 0 errors
  Test:   All 16 tests → all pass
  Audit:  Verify element_kind correctly propagates through chained assignments

STEP 5: Phase E-1e — For-loop + memory ops propagation
  ─────────────────────────────────────────────────
  Files: typechecker.cpp
  Change:
    - For: if range expression is an array, set loop var element_kind
    - Delete/Move/etc.: propagate element_kind from looked-up variable
  Build:  aurorac (Release) → 0 errors
  Test:   All 16 tests → all pass
  Audit:  Verify element_kind on range expression annotations
```

### Expected Build Targets

- `aurorac` (Release, MSVC x64) — primary build target
- All CTest targets (Release) — regression suite

### Expected Risks

1. **`infer_expr()` return type change (E-1c):** Medium risk — requires updating all callers (~10). Mitigation: use Option A (out parameter) for minimal caller disruption, or Option B with structured bindings.
2. **Array literal with no elements:** Edge case — empty `[]` has no first element. Element type remains Unknown (correct behaviour).
3. **Heterogeneous array literals:** `[1, "hello", 3.14]` — element type is ambiguous. Set to Unknown (correct — matches Aurora's dynamic semantics).
4. **Nested arrays:** `[[1, 2], [3, 4]]` — element_kind of outer array should be Array (inner array type). This requires recursive element type inference.

### Expected Review Checkpoints

1. After E-1a: verify no behavioural change (0 consumers read element_kind)
2. After E-1b: verify array literal element_kind set correctly for typed and untyped arrays
3. After E-1c: verify infer_expr caller updates are complete and correct
4. After E-1d: verify assignment chain element propagation
5. After E-1e: verify For-loop and memory operation propagation
6. Final: full build + test suite

---

## H) Validation Strategy

### Build Validation

```
cmake --build . --config Release         # 0 errors, 0 warnings
cmake --build . --config Debug           # 0 errors, 0 warnings (if Debug builds)
ctest -C Release                         # 16/17 pass (1 pre-existing skip)
```

### Behavioural Validation

Since `element_kind` has zero consumers, no behavioural changes are expected. Validation focuses on correctness of the metadata itself:

1. **Static code review:** Verify each `element_kind` assignment matches the inferred element type
2. **Print-based debugging (temporary):** Add a one-line `printf` to `annotate_node()` to log `element_kind` for array literals, verify against manual inspection
3. **Assertion checks:** Add `assert(node->type_annotation.element_kind != AstTypeKind::Unknown)` for array literals in debug builds (if desired)

### Regression Validation

```
ctest -C Release                         # All previously passing tests pass
# Manual test:
#   cat > /tmp/test.aura << EOF
#   x = [1, 2, 3]
#   for i in x
#       output(i)
#   EOF
#   aurorac /tmp/test.aura && ./a.out    # Should output 1 2 3
```

### Recommended Test Additions

While no new tests are strictly required for Phase E-1, the following would improve coverage:

1. **Type annotation print test:** Write a simple `.aura` file with array literals of different element types and verify via `--emit-llvm` or AST dump that element_kind is populated

---

## I) Execution Board Update

### Proposed Board Updates

| Change | Detail |
|--------|--------|
| **A-14** | Status → **DONE** (H2 Phases A-D2 complete) |
| **New: A-19** | H2 Phase E-1 — Element Kind Propagation |
| **New: A-20** | H2 Phase E-2 — Field-Level Annotations (future) |
| **New: A-21** | H2 Phase E-3 — gen_function() LLVM ABI (future) |

### Proposed Board Entry: A-19

```
A-19 | Task 21 | H2-E1 | P1 | Compiler / Type System
├── Current Status: NOT STARTED
├── Priority: P1
├── Area: Compiler / Type System / Annotations
├── Description: Populate AstTypeAnnotation::element_kind for all
│   compound type nodes (Array, List, Map, Set, Vector, Stack, Queue)
├── Depends on: A-14 (H2 D2 — COMPLETE)
├── Blocks: A-20 (E-2), A-21 (E-3)
├── Category: Feature (type metadata enrichment)
├── Report Item: H2 follow-up
├── Type: Patch
├── Estimated effort: ~20 edit sites, 3-4 files, ~60-80 LOC
├── Regression risk: VERY LOW (additive metadata, zero consumers)
├── Testing: All existing tests pass unchanged
└── Notes: element_kind exists as field but is never set by any write site.
    Phase E-1 fills this gap. BEHAVIOURAL CHANGE = NONE (no consumer reads
    element_kind yet). Pure metadata enrichment.
```

### Sub-task Breakdown

| Sub-phase | ID | Description | Edit sites | Status |
|-----------|----|-------------|------------|--------|
| E-1a | A-19a | annotate_node() element_type parameter | 4 | NOT STARTED |
| E-1b | A-19b | Array literal element type | 2 | NOT STARTED |
| E-1c | A-19c | infer_expr() element type carrier | 10 | NOT STARTED |
| E-1d | A-19d | Assignment/return element propagation | 5 | NOT STARTED |
| E-1e | A-19e | For-loop + memory ops element propagation | 5 | NOT STARTED |

### Dependent Tasks (for Phase E-1 context, not blockers)

| ID | Status | Description |
|----|--------|-------------|
| A-14 | ✅ DONE | H2 A-D2 (annotation pipeline foundation) |
| A-PRE3b | ⏳ DEFERRED | test_memory_safety linkage (no dependency on E-1) |
| A-PRE6 | ⏳ DEFERRED | #elif __APPLE__ (no dependency on E-1) |
| A-PRE7 | ⏳ DEFERRED | dead i386 macro (no dependency on E-1) |
| M2 | 🆕 UNSTARTED | find_last_of empty check (can be done anytime) |
| M4 | 🆕 UNSTARTED | Use-after-move (can be done anytime) |

---

## J) Final Recommendation

### Recommended Next Task

**H2 Phase E-1 — Element Kind Propagation** (Board ID: A-19)

### Reason

The H2 annotation pipeline is now complete for expression and declaration types (Phases A-D2). The single largest remaining metadata gap is that `AstTypeAnnotation::element_kind` — a field deliberately designed for this purpose — is never populated. Every array literal, list variable, map expression, and function return of compound type carries `element_kind::Unknown`.

Phase E-1 fills this gap with the **minimum possible risk profile**: pure additive metadata enrichment with zero behavioural change. No consumer reads `element_kind` today, so setting it from Unknown to a real value is guaranteed safe. The Typecker already has the element type information internally — it simply discards it before writing the annotation.

### Expected Effort

- **~20-25 edit sites** across 2-3 files
- **~60-80 new lines** of code
- **0 new files** needed
- **~2-3 hours** implementation time for an experienced developer
- **~1 hour** review time

### Expected Files Changed

| File | Edits | Change |
|------|-------|--------|
| `typechecker.cpp` | ~20 | annotate_node/ret signatures, infer_array, infer_expr, walk_stmt handlers |
| `typechecker_types.cpp` | ~3 | annotate_node_decl + type_alias registration |
| `typechecker.hpp` | ~1 | Optional: InferredType struct if Option B chosen |

### Estimated Edit Sites

~20-25 (details in Section 5.9)

### Regression Risk

**VERY LOW.** Element kind is checked by zero codegen consumers. Setting it from Unknown to a real value has no observable effect until a consumer is wired to read it. All 16 existing tests will continue to pass identically.

### Testing Required

- Full existing test suite: 16/17 pass (1 pre-existing skip) — no regressions expected
- Optional: Add verification tests that `element_kind` is correctly set for array literals

### Ready for Implementation

**YES**

The task is well-scoped, the data structures are in place, the pattern is established by H2 Phases A-D2, and there is zero regression risk. The implementation prompt can be generated immediately.

---

### Summary

```
Wave 1 (Security)          ✅ DONE: A-01 through A-03 (C1, C2, C3)
Wave 2 (Runtime safety)    ✅ DONE: A-04 through A-06 (C6, H4, C5 — 3 FPs)
Wave 3 (Portability)       ✅ DONE: A-07 through A-11 (C7, H1, H3, H6, H5)
Wave 4 (Architecture)      ✅ DONE: A-12 through A-14 (C4, H7, H2)
Wave 5 (Quality)           ✅ DONE: A-15 through A-18 (M1, M5, M7, M8)
Wave 6 (Maintenance)       ✅ DONE: A-PRE1 through A-PRE7 (5 DONE, 3 DEFERRED)

NEXT:  H2 Phase E-1 — Element Kind Propagation  (A-19, proposed)
       └── Pure metadata enrichment. Zero behavioural change.
       └── ~20 edit sites. ~70 LOC. Very low risk.
       └── Foundation for: field-level annotations, gen_function ABI, generics.
```

**This report recommends H2 Phase E-1 as the single highest-value next implementation task.** It builds on warm context, carries zero regression risk, has the highest architectural ROI of any available candidate, and is explicitly identified as the immediate next step in the H2 roadmap.
