#pragma once
#include "compiler/ir/ir.hpp"

/* ════════════════════════════════════════════════════════════
   ir_mem2reg — Promote alloca/load/store to SSA values
   ════════════════════════════════════════════════════════════
   Finds alloca/load/store patterns and replaces them with
   direct SSA references. Handles multi-block cases with
   phi insertion at merge points.
   ════════════════════════════════════════════════════════════ */

bool ir_mem2reg(IrFunction& fn, std::vector<IrType>& pool);
