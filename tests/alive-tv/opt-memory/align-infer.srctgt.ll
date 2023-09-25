; TEST-ARGS: -dbg

define void @src() {
  %p = alloca i64, align 4
  store i32 0, ptr %p, align 1
  %p2 = call ptr @llvm.ptrmask(ptr %p, i64 -8)
  store i32 0, ptr %p2, align 4
  ret void
}

define void @tgt() {
  %p = alloca i64, align 4
  store i32 0, ptr %p, align 4
  %p2 = call ptr @llvm.ptrmask(ptr %p, i64 -8)
  store i32 0, ptr %p2, align 8
  ret void
}

declare ptr @llvm.ptrmask(ptr, i64)

; CHECK: bits_byte: 32
