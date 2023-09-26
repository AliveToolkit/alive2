; TEST-ARGS: -tgt-is-asm
; SKIP-IDENTITY

define i32 @src() {
  %1 = alloca i8, i32 0, align 1
  store i8 0, ptr %1, align 1
  ret i32 0
}

define i32 @tgt() {
f:
  %stack = tail call dereferenceable(64) ptr @myalloc(i32 64)
  %0 = getelementptr inbounds i8, ptr %stack, i64 48
  %1 = ptrtoint ptr %0 to i64
  %a0_38 = add i64 %1, -16
  %2 = inttoptr i64 %a0_38 to ptr
  %3 = getelementptr i8, ptr %2, i64 15
  store i8 0, ptr %3, align 1
  ret i32 0
}

declare ptr @myalloc(i32) local_unnamed_addr allockind("alloc") allocsize(0)
