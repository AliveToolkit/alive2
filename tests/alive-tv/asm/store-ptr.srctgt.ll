; TEST-ARGS: -tgt-is-asm
; SKIP-IDENTITY

@dst = external global i64
@ptr = external global ptr

define void @src() {
   store ptr @dst, ptr @ptr, align 8
   ret void
}

define void @tgt() {
   store i64 ptrtoint (ptr @dst to i64), ptr @ptr, align 8
   ret void
}
