define void @src(i32* dereferenceable_or_null(4) %p) {
  ret void
}

define void @tgt(i32* dereferenceable_or_null(4) %p) {
  %c = icmp eq i32* %p, null
  br i1 %c, label %NOP, label %DEREFERENCE
NOP:
  ret void
DEREFERENCE:
  load i32, i32* %p, align 1
  ret void
}
