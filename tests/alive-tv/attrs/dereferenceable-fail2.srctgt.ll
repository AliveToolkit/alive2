; ERROR: Source is more defined than target

define void @src(ptr dereferenceable(1) %p) {
  %v = load i8, ptr %p
  %c = icmp ne i8 %v, 42
  br i1 %c, label %then, label %else

then:
  store i8 42, ptr %p
  ret void

else:
  ret void
}

define void @tgt(ptr dereferenceable(1) %p) {
  store i8 42, ptr %p
  ret void
}
