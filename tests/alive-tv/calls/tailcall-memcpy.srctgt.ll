define i64 @src(i64 noundef %arg) {
  %call2 = alloca i64
  %call3 = alloca i64
  store i64 %arg, ptr %call2, align 8
  tail call void @llvm.memcpy.p0.p0.i64(ptr %call3, ptr %call2, i64 8, i1 false)
  %load1 = load i64, ptr %call3, align 8
  ret i64 %load1
}

define i64 @tgt(i64 noundef %arg) {
  %call2 = alloca i64
  %call3 = alloca i64
  store i64 %arg, ptr %call2, align 8
  tail call void @llvm.memcpy.p0.p0.i64(ptr %call3, ptr %call2, i64 8, i1 false)
  ret i64 poison
}

declare void @llvm.memcpy.p0.p0.i64(ptr, ptr, i64, i1)
