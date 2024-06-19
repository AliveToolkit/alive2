define void @src(ptr writable dereferenceable(4) %p) {
  %v = load i32, ptr %p
  %c = icmp ne i32 %v, 42
  br i1 %c, label %then, label %else

then:
  store i32 42, ptr %p
  ret void

else:
  ret void
}

define void @tgt(ptr writable dereferenceable(4) %p) {
  store i32 42, ptr %p
  ret void
}
