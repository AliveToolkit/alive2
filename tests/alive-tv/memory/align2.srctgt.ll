; ERROR: Value mismatch

define i32 @src(i32* align(4) %p) {
  call void @g(i32* align(2) %p)
  %v = load i32, i32* %p, align 4
  ret i32 %v
}

define i32 @tgt(i32* align(4) %p) {
  call void @g(i32* align(2) %p)
  %v = load i32, i32* %p, align 4
  ret i32 0
}

declare void @g(i32*)
