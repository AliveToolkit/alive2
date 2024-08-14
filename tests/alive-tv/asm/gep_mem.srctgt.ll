; TEST-ARGS: -tgt-is-asm
@G = external global ptr, align 8

define void @src(ptr %0) {
   %LGV = load ptr, ptr %0, align 8
   %G1 = getelementptr i8, ptr %LGV, i64 8
   store ptr %G1, ptr @G, align 8
   ret void
}

define void @tgt(ptr %0) {
   %a3_1 = load i64, ptr %0, align 8
   %a6_1 = add i64 %a3_1, 8
   store i64 %a6_1, ptr @G, align 8
   ret void
}
