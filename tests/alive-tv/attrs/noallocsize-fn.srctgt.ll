; ERROR: Source and target don't have the same return domain

define i64 @src() {
  %stack = call ptr @myalloc()
  %sz = call i64 @llvm.objectsize.i64.p0(ptr %stack, i1 false, i1 false, i1 false)
  ret i64 %sz
}

define i64 @tgt() {
  ret i64 -1
}

declare ptr @myalloc() allockind("alloc")
declare i64 @llvm.objectsize.i64.p0(ptr, i1, i1, i1)
