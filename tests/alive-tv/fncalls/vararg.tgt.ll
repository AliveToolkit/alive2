target triple = "x86_64-unknown-linux-gnu"

declare void @g(i8*, ...)
define void @f() {
  tail call void (i8*, ...) @g(i8* null)
  ret void
}
