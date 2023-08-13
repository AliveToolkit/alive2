; TEST-ARGS: -dbg

define i32 @calloc_init(i32 %x) {
  %ptr = call noalias ptr @calloc(i64 1, i64 4)
  store i32 %x, ptr %ptr, align 4
  %v = load i32, ptr %ptr, align 4
  call void @free(ptr %ptr)
  ret i32 %v
}

; CHECK: bits_byte: 32

declare noalias ptr @calloc(i64, i64)
declare void @free(ptr)
