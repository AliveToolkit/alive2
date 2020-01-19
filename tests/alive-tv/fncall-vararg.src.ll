target triple = "x86_64-unknown-linux-gnu"

declare void @g(i8*, ...)
define void @f() {
  tail call void (i8*, ...) @g(i8* null, i32 0)
  ret void
}
; ERROR: Source is more defined than target
