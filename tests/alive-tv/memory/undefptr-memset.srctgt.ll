; TEST-ARGS: -smt-to=10000

define void @src(i32** %ptr) {
  store i32* undef, i32** %ptr
  ret void
}

define void @tgt(i32** %ptr) {
  %p = bitcast i32** %ptr to i8*
  call void @llvm.memset.p0i8.i32(i8* %p, i8 undef, i32 8, i1 0)
  ret void
}

declare void @llvm.memset.p0i8.i32(i8*, i8, i32, i1)
