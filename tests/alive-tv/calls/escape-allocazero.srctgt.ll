; from test/Transforms/InstCombine/alloca.ll
declare void @f([0 x i8]*)

define void @src() {
  %p = alloca [0 x i8]
  call void @f([0 x i8]* %p)
  %q = alloca [0 x i8]
  call void @f([0 x i8]* %q)
  ret void
}

define void @tgt() {
  %p = alloca [0 x i8]
  call void @f([0 x i8]* %p)
  call void @f([0 x i8]* %p)
  ret void
}

; XFAIL: Precondition is always false
