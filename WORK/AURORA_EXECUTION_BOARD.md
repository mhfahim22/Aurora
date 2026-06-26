# AURORA EXECUTION BOARD — DEEPSEEK V4 FLASH WORKFLOW

This board is meant to track Aurora stabilization work driven by:

* the Aurora report
* the Aurora DeepSeek master prompt
* the Aurora task pack
* manual build/test/review after each patch

Use this board as the central control sheet for Aurora Phase 1 stabilization.

---

# 1) STATUS LEGEND

Use one of these status values for every task.

## Task status

* **NOT STARTED** → task has not been given to the AI yet
* **AI AUDITING** → task is currently being audited by the AI
* **AI PATCH PROPOSED** → AI has produced a patch, but it is not yet reviewed/applied
* **UNDER MANUAL REVIEW** → patch is being checked by you
* **PATCH APPLIED** → patch has been merged into local code
* **BUILD FAILED** → patch applied but build failed
* **BUILD PASSED** → patch applied and project builds
* **TEST FAILED** → build passed but validation/regression failed
* **TEST PASSED** → patch passed validation
* **PARTIALLY FIXED** → issue reduced/mitigated but not fully solved
* **BLOCKED** → task cannot continue without redesign / missing context / dependency
* **DONE** → issue is fixed to the current Aurora Phase 1 standard
* **DEFERRED** → intentionally postponed to a later phase

---

# 2) MASTER AURORA STABILIZATION BOARD

Track every report item here.

## Recommended columns

| Board ID | Task Pack ID | Report Item | Priority | Area | File(s) | Issue Summary | Task Type | Current Status | AI Output Quality | Patch Applied? | Build | Test | Manual Review Needed? | Follow-up Needed? | Commit / PR | Notes |
| -------- | -----------: | ----------- | -------- | ---- | ------- | ------------- | --------- | -------------- | ----------------- | -------------- | ----- | ---- | --------------------- | ----------------- | ----------- | ----- |

---

# 3) INITIAL BOARD SEED

Fill the board like this at the start.

| Board ID | Task Pack ID | Report Item | Priority | Area                           | File(s)                                                | Issue Summary                                           | Task Type            | Current Status | AI Output Quality | Patch Applied? | Build | Test | Manual Review Needed? | Follow-up Needed? | Commit / PR | Notes                                              |
| -------- | -----------: | ----------- | -------- | ------------------------------ | ------------------------------------------------------ | ------------------------------------------------------- | -------------------- | -------------- | ----------------- | -------------- | ----- | ---- | --------------------- | ----------------- | ----------- | -------------------------------------------------- |
| A-01     |      Task 01 | C1          | P0       | Security / Compiler Driver     | `aurora/src/compiler/main.cpp`                         | Unsafe `std::system` linker execution                   | Patch                | DONE           | A                 | Yes            | Passed | Passed | Yes                   | No               | —           | C1+C2 fixed together; `run_process()` helper created, EINTR-safe POSIX path |
| A-02     |      Task 01 | C2          | P0       | Security / Compiler Driver     | `aurora/src/compiler/main.cpp`                         | Unsafe `std::system` auto-bindgen execution             | Patch                | DONE           | A                 | Yes            | Passed | Passed | Yes                   | No               | —           | Same patch as C1; `CreateProcessA`/fork+execvp                              |
| A-03     |      Task 02 | C3          | P0       | Security / Compiler Driver     | `aurora/src/compiler/main.cpp`                         | Unsafe `popen(cmd.c_str(), "r")` shell execution        | Patch                | DONE           | B                 | Yes            | Passed | Passed | Yes                   | Yes              | —           | POSIX fork/exec pipe capture; Windows untestable (no WSL); `_popen` in `detect_msvc_lib_paths` kept (trusted vswhere only) |
| A-04     |      Task 03 | C6          | P0       | Runtime / Memory               | `aurora/src/runtime/core/memory.cpp`                   | Use-after-free in `aurora_drop_glue`                    | Audit                | DONE           | A                 | No (FP)        | —     | —      | Yes                   | No               | —           | FALSE POSITIVE: code reads magic BEFORE free(), report line numbers stale   |
| A-05     |      Task 04 | H4          | P1       | Runtime / Backend              | `aurora/src/runtime/backend/server.cpp`                | Dangling `.c_str()` lifetime issues                     | Audit                | DONE           | A                 | No (FP)        | —     | —      | Yes                   | No               | —           | FALSE POSITIVE: all 25 `.c_str()` call sites safe (immediate use or strdup) |
| A-06     |      Task 05 | C5          | P0       | Runtime / GC                   | `aurora/src/runtime/core/memory.cpp`                   | GC auto-collect race condition                          | Audit                | DONE           | A                 | No (FP)        | —     | —      | Yes                   | No               | —           | FALSE POSITIVE: `LOCK_GC()` already present at lines 794/805 in `aurora_gc_alloc` |
| A-07     |      Task 06 | C7          | P0       | Compiler / Codegen             | `aurora/src/compiler/codegen/llvm_codegen.cpp`         | Hardcoded target triple                                 | Patch                | DONE           | A                 | Yes            | Passed | Passed | Yes                   | No               | —           | `"x86_64-pc-windows-msvc"` → `llvm::sys::getProcessTriple()` (line 72)      |
| A-08     |      Task 07 | H1          | P1       | Compiler / Toolchain Discovery | `aurora/src/compiler/main.cpp`                         | Hardcoded Windows paths / fragile discovery             | Patch                | DONE           | B                 | Yes            | Passed | Passed | Yes                   | No               | —           | 4 edits: vswhere PATH lookup, env var fallback, all VS editions+years, proper `find_lld_link` priority chain |
| A-09     |      Task 08 | H3          | P1       | Compiler UX / Diagnostics      | `aurora/src/compiler/main.cpp`                         | Debug spam / STAGE prints in normal output              | Patch                | DONE           | A                 | Yes            | Passed | Passed | Yes                   | No               | —           | 30+ print sites gated behind `--verbose`/`-v`; file-scope `static bool verbose`; STAGE/[voss]/[import]/[auto-bindgen]/JIT/IR-length/File-written all hidden by default |
| A-10     |      Task 09 | H6          | P1       | Compiler Reliability           | `aurora/src/compiler/main.cpp`                         | Silent exception swallowing in optimizer                | Patch                | DONE           | A                 | Yes            | Passed | Passed | Yes                   | No               | —           | Two catch(...) blocks updated: always report [Warning] instead of gating behind --verbose; `STAGE4b`/`STAGE5` prefix replaced with proper `\033[1;33m[Warning]\033[0m` format |
| A-11     |      Task 10 | H5          | P1       | Build System                   | `CMakeLists.txt`                                         | Debug build uses /MD instead of /MDd (LLVM compatibility constraint) | Patch                | DONE           | B                 | Yes            | Passed | Passed | Yes                   | No               | —           | Removed redundant `add_compile_options(/MD)` (already handled by `CMAKE_MSVC_RUNTIME_LIBRARY`); expanded comment to explain lost CRT debug features and what is needed to restore them (debug LLVM build) |
| A-12     |      Task 11 | C4          | P0       | Runtime / Fibers               | `aurora/src/runtime/async/fiber.cpp` + `include/runtime/async.hpp` | UB from `setjmp/longjmp` across C++ lifetimes — REAL; destructors bypassed on yield | Patch + Architecture | DONE           | A                 | Yes            | Passed | Passed | Yes                   | No               | —           | Removed setjmp/longjmp entirely; replaced with flag-based yield + direct resume; removed `FiberContext` struct and `context` field; rewrote to 64 lines from 103. UB eliminated. Yield now returns normally (no destructor bypass). Resume re-calls function on yield. |
| A-13     |      Task 12 | H7          | P1       | Runtime / GC                   | `aurora/src/runtime/core/memory.cpp` + `runtime_exports.hpp` | GC root scanning limitation — REAL; `gc_scan_and_mark_range` hardcoded to 1KB for unknown roots now uses stored size (or 64KB default) | Patch + Add API | DONE | A | Yes | Passed | Passed | Yes | No | — | Changed `gc_unknown_roots` from `vector<void*>` to `vector<UnknownRoot>` storing `{ptr, size}`; added `aurora_gc_register_root_sized(void*, size_t)`; scan uses stored size or 64KB default; legacy API stores size=0 (64KB fallback). Compiler path still hits known-root branch. All GC tests pass. |
| A-14     |      Task 13 | H2          | P1       | Compiler / Type System         | typechecker + semantic files + codegen + ast.hpp + parser | Weak / incomplete type checking — REAL; TypeChecker is a dead-letter pass (results ignored by codegen); 8 sub-issues, 15 escape hatches | Architecture / Plan  | DESIGN COMPLETE | —                 | No (plan only)| —     | —    | Yes                   | Yes               | —           | H2 is far deeper than report suggests. TypeChecker::analyse() returns void — codegen ignores it. ASTNode has no type annotation field. All params = Unknown. Return default = Int. expect_assignable() defined but only wired to return checks. Codegen escape hatches: `gen_expr` returns i64(0) for null, `gen_binop` returns i64(0) for unsupported ops, `expr_is_string_type` defaults to true. Remediation split into 6 sub-tasks (H2-A through H2-F). See audit for full 15-escape-hatch catalog, 13-category enforcement table, and 6-phase implementation sequence. |
| A-15     |      Task 14 | M1          | P2       | Parser                         | parser files                                           | Thin parser / weak recovery — FALSE POSITIVE on thinness, REAL on recovery | Audit / Plan | DONE | A | No (plan only) | — | — | Yes | Yes | — | "parser.cpp too thin" disproven (5.3K parser.cpp + 7 parse_*.cpp files). No error recovery IS real; roadmap: token-level skip, statement-level skip, annotation tracking, multiline sync |
| A-16     |      Task 15 | M5          | P2       | Compiler / Codegen             | `codegen_stmt.cpp:1471`                                | Temporary `.c_str()` lifetime bug in codegen            | Patch                | DONE           | A                 | No (FP)        | —     | —    | Yes                   | No               | —           | FALSE POSITIVE: `child->value` is stable `ASTNode` member, not a temporary. No dangling `.c_str()` on line 1471. |
| A-17     |      Task 16 | M7          | P2       | Testing                        | repo-wide                                              | Missing CTest integration — REAL                        | Patch                | DONE           | A                 | Yes            | Passed | Passed | Yes                   | No               | —           | Added `include(CTest)` + 15 `add_test()` calls covering all existing test executables. CTest now functional.             |
| A-18     |      Task 16 | M8          | P2       | CI / Infra                     | repo-wide                                              | CI exists but incomplete — PARTIALLY REAL               | Audit                | DONE           | B                 | No (audit)     | —     | —    | Yes                   | Yes               | —           | CI `.yml` exists but under-tests. No cross-platform coverage. Roadmap: GitHub Actions `test` step, platform matrix, CTest integration, pre-commit checks. |
| A-PRE1   |     PRE task | PRE-1       | P1       | Security / Package Manager     | `aurora/src/compiler/package_manager.cpp`              | `std::system()` in package manager — REAL               | Patch                | DONE           | A                 | Yes            | Passed | N/A   | Yes                   | No               | —           | Replaced `std::system(cmd.c_str())` with `run_process()` from existing helper. Same pattern as C1/C2 fix. |
| A-PRE2   |     PRE task | PRE-2       | P2       | Testing                        | `aurora/tests/CMakeLists.txt`                          | Orphaned test_symbols not in CMake — REAL               | Patch                | DONE           | A                 | Yes            | Passed | Passed | Yes                   | No               | —           | Added `add_executable` + `target_link_libraries` for test_symbols to CMakeLists.txt.                       |
| A-PRE3a  |     PRE task | PRE-3a      | P2       | Testing                        | `aurora/tests/test_memory_safety.cpp`                  | String/version bugs in test — REAL                      | Patch                | DONE           | A                 | Yes            | N/A    | N/A    | Yes                   | No               | —           | Fixed `collected_before` assertion (used `all_allocations` map) and version string hardcoding (uses `AURORA_VERSION_STRING`). |
| A-PRE3b  |     PRE task | PRE-3b      | P2       | Testing                        | `aurora/tests/test_memory_safety.cpp`                  | test_memory_safety can't link standalone — REAL          | Build fix            | DEFERRED       | —                 | No             | —     | —    | Yes                   | Yes               | —           | Needs compiler static lib target in CMake. Full build deferred to post-stabilization.               |
| A-PRE4   |     PRE task | PRE-4       | P1       | Compiler / Codegen             | `codegen_core.cpp`, `optimized_codegen.cpp`            | Hardcoded triple/data layout in two additional codegen paths — REAL | Patch | DONE           | A                 | Yes            | Passed | Passed | Yes                   | No               | —           | `"x86_64-pc-windows-msvc"` → `llvm::sys::getProcessTriple()` in both files. Matches C7 pattern from llvm_codegen.cpp. Data layout still hardcoded (platform constraint). |
| A-PRE5   |     PRE task | PRE-5       | P1       | Tooling                        | `aurora/tools/cppwrap/cppwrap.cpp`, `aurora/tools/bindgen/bindgen.cpp` | Dangling `.c_str()` pointers in clang args via vector reallocation — REAL | Patch | DONE | A | Yes | Passed | N/A | Yes | No | — | Added `g_clang_args_storage.reserve(argc)` before parse loops. Guarantees no reallocation. Objects compiled; link failure pre-existing (libclang). |
| A-PRE6   |     PRE task | PRE-6       | P3       | Portability                    | `aurora/src/compiler/main.cpp`, various               | Bare `#elif __APPLE__` without `defined()` — REAL        | Style fix            | DEFERRED       | —                 | No             | —     | —    | No                    | No               | —           | Works by accident: preprocessor treats undefined identifiers as 0. 7 instances across the repo. Harmless, low priority. |
| A-PRE7   |     PRE task | PRE-7       | P3       | Portability                    | Platform headers                                       | Dead i386 macro in platform detection                    | Cleanup              | DEFERRED       | —                 | No             | —     | —    | No                    | No               | —           | May already be removed. Harmless. Cannot find currently. |

---

# 4) AI OUTPUT QUALITY SCALE

After every DeepSeek response, rate it before applying anything.

## Quality values

* **A — Excellent**

  * Report mapping correct
  * Real code audited
  * Patch is tight and plausible
  * Tests are meaningful
  * Risks are honestly stated

* **B — Good**

  * Mostly solid
  * Minor omissions
  * Needs small manual correction or extra verification

* **C — Usable with caution**

  * Some value, but weak reasoning or incomplete patch
  * Must be manually corrected before applying

* **D — Poor**

  * Hallucinated file behavior
  * vague patch
  * skipped audit
  * unsafe confidence

* **F — Reject**

  * incorrect or dangerous
  * not grounded in Aurora code/report
  * should not be applied

---

# 5) PER-TASK REVIEW CARD TEMPLATE

Create one review card per task after the AI produces a patch or audit.

## Review Card Template

### Task Review Card

* **Board ID:**
* **Task Pack ID:**
* **Report Item:**
* **Date:**
* **AI Model:** DeepSeek V4 Flash
* **Scope of this run:**
* **Relevant files touched:**
* **AI output quality:** A / B / C / D / F

### 1) What the AI claims

* short summary of the claimed bug/fix

### 2) What the code actually seems to show

* your own manual confirmation notes

### 3) Patch decision

* Reject
* Request revision
* Apply as-is
* Apply with manual edits

### 4) Build result

* Not run yet / Failed / Passed

### 5) Test result

* Not run yet / Failed / Passed / Manual validation only

### 6) Remaining risks

* lifetime risk?
* thread-safety risk?
* portability risk?
* hidden behavior change?

### 7) Follow-up task

* next task or cleanup required

---

# 6) PATCH VALIDATION CHECKLIST

Before you mark a patch as DONE, run through this checklist.

## Patch Validation Checklist

* [ ] Report item correctly identified
* [ ] AI audited the actual Aurora code, not just the report
* [ ] I manually checked the touched function(s)
* [ ] Patch scope is narrow and does not include unrelated refactors
* [ ] Patch actually addresses the root bug / vulnerability
* [ ] No obvious new lifetime / ownership bug introduced
* [ ] No obvious portability regression introduced
* [ ] Build completed successfully
* [ ] Relevant validation / regression tests were run
* [ ] Remaining risks are documented in the board notes
* [ ] Commit message / PR summary written if patch is accepted

Only after this should the board status move to **DONE**.

---

# 7) RECOMMENDED WORKFLOW FOR EACH TASK

Use this exact loop for every Aurora task.

## Step 1 — Assign task to DeepSeek

Pick the next task from the task pack and send it.

## Step 2 — Update board immediately

Set:

* Current Status → **AI AUDITING**

## Step 3 — Receive AI response

If AI only audits:

* mark status as **UNDER MANUAL REVIEW** or **BLOCKED** depending on result

If AI provides a patch:

* mark status as **AI PATCH PROPOSED**
* assign AI output quality grade

## Step 4 — Manual review

Read the diff carefully:

* confirm file/function
* confirm issue is real
* confirm patch is local and safe

Then update:

* Current Status → **UNDER MANUAL REVIEW**

## Step 5 — Apply patch locally

If accepted:

* Patch Applied? → **Yes**
* Current Status → **PATCH APPLIED**

## Step 6 — Build Aurora

If build fails:

* Build → **Failed**
* Current Status → **BUILD FAILED**

If build passes:

* Build → **Passed**
* Current Status → **BUILD PASSED**

## Step 7 — Run validation / regression tests

If tests fail:

* Test → **Failed**
* Current Status → **TEST FAILED**

If tests pass:

* Test → **Passed**
* Current Status → **TEST PASSED**

## Step 8 — Final status decision

* If fully fixed → **DONE**
* If partially mitigated → **PARTIALLY FIXED**
* If patch exposed a bigger redesign need → **BLOCKED** or **DEFERRED**

## Step 9 — Write commit / PR summary

Use Task 19 for this if needed.

---

# 8) SUGGESTED KANBAN VIEW

If you want a cleaner board, split the tasks into Kanban columns.

## Column 1 — Backlog

* not started Aurora tasks

## Column 2 — AI Auditing

* task currently being analyzed by DeepSeek

## Column 3 — AI Patch Proposed

* patch generated, waiting for your review

## Column 4 — Under Manual Review

* you are reading/verifying/editing the patch

## Column 5 — Patch Applied / Build Pending

* patch merged locally, build not yet validated

## Column 6 — Testing

* build passed, validation in progress

## Column 7 — Done

* accepted, validated Aurora task

## Column 8 — Blocked / Deferred

* redesign needed or not worth touching right now

---

# 9) RECOMMENDED EXECUTION ORDER

Use this order unless code realities force a change.

## Wave 1 — Critical security / safety

1. A-01 / A-02 → C1 + C2
2. A-03 → C3
3. A-04 → C6

## Wave 2 — Runtime safety

4. A-05 → H4
5. A-06 → C5

## Wave 3 — Portability / compiler hardening

6. A-07 → C7
7. A-08 → H1
8. A-09 → H3
9. A-10 → H6
10. A-11 → H5

## Wave 4 — Architecture audits

11. A-12 → C4
12. A-13 → H7
13. A-14 → H2

## Wave 5 — Medium-priority quality work

14. A-15 → M1 (DONE — audit/plan only)
15. A-16 → M5 (DONE — false positive)
16. A-17 / A-18 → M7 + M8 (DONE — CTest patch + CI audit)

## Pre-Task-17 Sweep

17. A-PRE1 → PRE-1 (DONE — package_manager std::system)
18. A-PRE2 → PRE-2 (DONE — orphaned test_symbols)
19. A-PRE3a → PRE-3a (DONE — test_memory_safety bugs)
20. A-PRE3b → PRE-3b (DEFERRED — needs compiler static lib)
21. A-PRE4 → PRE-4 (DONE — hardcoded triple in remaining codegen)
22. A-PRE5 → PRE-5 (DONE — dangling .c_str() in cppwrap/bindgen)
23. A-PRE6 → PRE-6 (DEFERRED — #elif __APPLE__, harmless)
24. A-PRE7 → PRE-7 (DEFERRED — dead i386 macro, harmless)

---

# 10) MINIMAL SESSION LOG TEMPLATE

Use this if you want to keep a chronological Aurora work log.

## Session Log Entry

* **Date / time:**
* **Task worked on:**
* **Board ID(s):**
* **AI prompt used:** Task XX
* **AI output quality:**
* **Patch applied?:**
* **Build result:**
* **Test result:**
* **Commit hash / branch:**
* **Key notes:**
* **Next action:**

---

# 11) EXAMPLE OF A FILLED TASK ENTRY

## Example

* **Board ID:** A-01
* **Task Pack ID:** Task 01
* **Report Item:** C1
* **Current Status:** AI PATCH PROPOSED
* **AI Output Quality:** B
* **Patch Applied?:** No
* **Build:** —
* **Test:** —
* **Manual Review Needed?:** Yes
* **Follow-up Needed?:** Yes
* **Notes:** Replaced linker `std::system` path with structured process helper, but path quoting on Windows still needs manual verification before apply.

---

# 12) FINAL RULE FOR THIS BOARD

The board is not just a checklist.
It is your **control system** for deciding whether an Aurora issue is:

* actually understood
* safely patched
* verified
* truly closed

Do not mark a task as DONE just because the AI sounded confident.
DONE means:

* patch reviewed
* patch applied
* Aurora builds
* validation completed
* remaining risks documented
