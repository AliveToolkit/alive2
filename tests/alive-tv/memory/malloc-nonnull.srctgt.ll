define i1 @src() {
  %m = call nonnull i8* @malloc(i64 4)
  %n = call nonnull i8* @malloc(i64 4)
  call void @unknown(i8* %n)
  %cmp = icmp eq i8* %m, %n
  ret i1 %cmp
}

define i1 @tgt() {
  %n = call nonnull i8* @malloc(i64 4)
  call void @unknown(i8* %n)
  ret i1 false
}

declare i8* @malloc(i64)
declare void @unknown(i8*)
