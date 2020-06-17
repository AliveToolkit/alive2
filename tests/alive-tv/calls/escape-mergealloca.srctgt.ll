; from test/Transforms/InstCombine/alloca.ll
declare void @f(i32*)

define void @src() {
  %p = alloca i32
  call void @f(i32* %p)
  %q = alloca i32
  call void @f(i32* %q)
  ret void
}

define void @tgt() {
  %p = alloca i32
  call void @f(i32* %p)
  call void @f(i32* %p)
  ret void
}

; ERROR: Source is more defined than target
