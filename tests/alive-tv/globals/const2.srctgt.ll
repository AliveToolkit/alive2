@glbl = constant i32 10

define i32 @src(ptr %ptr) {
  %A = load i32, ptr %ptr
  %B = icmp eq ptr %ptr, @glbl
  %C = select i1 %B, i32 %A, i32 10
  ret i32 %C
}

define i32 @tgt(ptr %ptr) {
	ret i32 10
}
