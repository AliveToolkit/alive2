define void @src() {
  %c = alloca i8, align 1
  call i32 @g(i32* undef)
  ret void
}

define void @tgt() {
  call i32 @g(i32* undef)
  ret void
}

declare i32 @g(i32*)
