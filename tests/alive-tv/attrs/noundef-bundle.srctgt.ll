declare void @llvm.assume(i1)

define i32 @src(i32 %x) {
  %x.fr = freeze i32 %x
  call void @llvm.assume(i1 1) [ "noundef"(i32 %x) ]
  ret i32 %x.fr
}

define i32 @tgt(i32 %x) {
  ret i32 %x
}
