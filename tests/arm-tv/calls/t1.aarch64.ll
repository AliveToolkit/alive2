; XFAIL: 

define i32 @f(i32 noundef %x) {
entry:
  tail call void @g()
  %mul = shl nsw i32 %x, 1
  ret i32 %mul
}

declare void @g()

