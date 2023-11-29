; ERROR: Source is more defined than target

define void @src(i32 %align) {
  call void @llvm.assume(i1 true) ["align"(ptr null, i32 5)]
  call void @llvm.assume(i1 true) ["align"(ptr null, i32 %align)]
  ret void
}

define void @tgt(i32 %align) {
  unreachable
}

declare void @llvm.assume(i1)
