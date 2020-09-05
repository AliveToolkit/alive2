@glbl = constant i32 10

define i32 @test61(i32* %ptr) {
  %A = load i32, i32* %ptr
  %B = icmp eq i32* %ptr, @glbl
  %C = select i1 %B, i32 %A, i32 10
  ret i32 %C
}

define i32 @tgt(i32* %ptr) {
	ret i32 10
}
