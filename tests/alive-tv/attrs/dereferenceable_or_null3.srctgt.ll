define void @src(ptr dereferenceable_or_null(4) %p) {
  ret void
}

define void @tgt(ptr dereferenceable_or_null(4) %p) {
  %c = icmp eq ptr %p, null
  br i1 %c, label %NOP, label %DEREFERENCE
NOP:
  ret void
DEREFERENCE:
  load i32, ptr %p, align 1
  ret void
}
