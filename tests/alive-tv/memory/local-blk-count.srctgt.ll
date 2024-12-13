define void @src(ptr %0, i32 %1) {
  %3 = call ptr @memrchr(ptr %0, i32 %1, i64 7)
  ret void
}

define void @tgt(ptr %0, i32 %1) {
  %stack = call ptr @myalloc()
  call ptr @memrchr(ptr %0, i32 %1, i64 7)
  ret void
}

declare ptr @memrchr(ptr, i32, i64)
declare ptr @myalloc() allockind("alloc")
