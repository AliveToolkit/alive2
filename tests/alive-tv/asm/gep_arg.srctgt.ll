; TEST-ARGS: -tgt-is-asm
; SKIP-IDENTITY

define ptr @src(ptr noundef %0, i64 noundef %1) {
2:
   %_t = trunc i64 %1 to i32
   %3 = getelementptr inbounds i8, ptr %0, i32 8
   store i32 %_t, ptr %3, align 4
   ret ptr %3
}

define ptr @tgt(ptr noundef %0, i64 noundef %1) {
i32_8:
   %2 = ptrtoint ptr %0 to i64
   %a0_38 = freeze i64 %1
   %a0_39 = trunc i64 %a0_38 to i32
   %a0_44 = add i64 %2, 8
   %3 = inttoptr i64 %a0_44 to ptr
   store i32 %a0_39, ptr %3, align 1
   ret ptr %3
}
