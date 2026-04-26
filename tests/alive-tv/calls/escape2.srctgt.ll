@glb = global ptr null
declare ptr @f(ptr %p)

declare void @llvm.lifetime.start(i64, ptr captures(none))
declare void @llvm.lifetime.end(i64, ptr captures(none))

define i8 @src() {
  %p = alloca i8
  call void @llvm.lifetime.start(i64 1, ptr %p)
  store i8 1, ptr %p

  %q = load ptr, ptr @glb
  %q2 = call ptr @f(ptr %q) ; cannot be %p
  store i8 2, ptr %q2

  %v = load i8, ptr %p
  call void @llvm.lifetime.end(i64 1, ptr %p)
  ret i8 %v
}

define i8 @tgt() {
  %p = alloca i8
  call void @llvm.lifetime.start(i64 1, ptr %p)
  store i8 1, ptr %p

  %q = load ptr, ptr @glb
  %q2 = call ptr @f(ptr %q) ; cannot be %p
  store i8 2, ptr %q2

  call void @llvm.lifetime.end(i64 1, ptr %p)
  ret i8 1
}
