; ModuleID = 'aurora_module'
source_filename = "aurora_module"
target datalayout = "e-m:w-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-windows-msvc"

@strlit_ = private unnamed_addr constant [9 x i8] c"count = \00", align 1

declare ptr @aurora_arena_alloc(i64) local_unnamed_addr

declare void @aurora_print_str(ptr) local_unnamed_addr

declare ptr @aurora_str_from_cstr(ptr) local_unnamed_addr

declare ptr @aurora_str_append(ptr, ptr) local_unnamed_addr

declare ptr @aurora_int_to_str(i64) local_unnamed_addr

; Function Attrs: mustprogress nofree norecurse nosync nounwind willreturn memory(none)
define noundef i32 @__entry(i32 %0, ptr nocapture readnone %1) local_unnamed_addr #0 {
entry:
  ret i32 0
}

; Function Attrs: mustprogress nofree norecurse nosync nounwind willreturn memory(none)
define i64 @accum(i64 %a, i64 %b) local_unnamed_addr #0 {
entry:
  %add = add nsw i64 %b, %a
  ret i64 %add
}

define noundef i64 @main_user() local_unnamed_addr {
entry:
  %count_arena = tail call ptr @aurora_arena_alloc(i64 8)
  store i64 0, ptr %count_arena, align 8
  %aurora_lit = tail call ptr @aurora_str_from_cstr(ptr nonnull @strlit_)
  %count_arena.promoted = load i64, ptr %count_arena, align 8
  %0 = add i64 %count_arena.promoted, 1000000000
  store i64 %0, ptr %count_arena, align 8
  %str_ret = tail call ptr @aurora_int_to_str(i64 %0)
  %strcat_result = tail call ptr @aurora_str_append(ptr %aurora_lit, ptr %str_ret)
  tail call void @aurora_print_str(ptr %strcat_result)
  ret i64 0
}

define noundef i32 @main(i32 %argc, ptr nocapture readnone %argv) local_unnamed_addr {
entry:
  %count_arena.i = tail call ptr @aurora_arena_alloc(i64 8)
  store i64 0, ptr %count_arena.i, align 8
  %aurora_lit.i = tail call ptr @aurora_str_from_cstr(ptr nonnull @strlit_)
  %count_arena.promoted.i = load i64, ptr %count_arena.i, align 8
  %0 = add i64 %count_arena.promoted.i, 1000000000
  store i64 %0, ptr %count_arena.i, align 8
  %str_ret.i = tail call ptr @aurora_int_to_str(i64 %0)
  %strcat_result.i = tail call ptr @aurora_str_append(ptr %aurora_lit.i, ptr %str_ret.i)
  tail call void @aurora_print_str(ptr %strcat_result.i)
  ret i32 0
}

attributes #0 = { mustprogress nofree norecurse nosync nounwind willreturn memory(none) }

