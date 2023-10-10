%struct.d = type { ptr, ptr }

@f = external global ptr, align 8

define void @src() {
entry:
  %0 = load ptr, ptr @f, align 8
  %c = getelementptr inbounds %struct.d, ptr %0, i32 0, i32 1
  call void @e(ptr %c)
  inttoptr i64 42 to ptr
  ret void
}

define void @tgt() {
entry:
  %0 = load ptr, ptr @f, align 8
  %c = getelementptr inbounds %struct.d, ptr %0, i32 0, i32 1
  call void @e(ptr nonnull %c)
  ret void
}

declare void @e(ptr)
