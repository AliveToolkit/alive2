define i1 @src() {
  %m = call nonnull ptr @malloc(i64 4)
  %n = call nonnull ptr @malloc(i64 4)
  call void @unknown(ptr %n)
  %cmp = icmp eq ptr %m, %n
  ret i1 %cmp
}

define i1 @tgt() {
  %n = call nonnull ptr @malloc(i64 4)
  call void @unknown(ptr %n)
  ret i1 false
}

declare ptr @malloc(i64)
declare void @unknown(ptr)
