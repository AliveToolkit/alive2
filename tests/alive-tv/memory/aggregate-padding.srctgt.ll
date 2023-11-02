@glb = global {i8, i32} poison, align 4

define i8 @src() {
  store i32 0, ptr @glb, align 4 ; fill padding with 0

  %v = load {i8, i32}, ptr @glb ;padding should be poison
  store {i8, i32} %v, ptr @glb, align 4

  %p3 = getelementptr i8, ptr @glb, i64 1
  %padding = load i8, ptr %p3
  ret i8 %padding
}

define i8 @tgt() {
  store i32 0, ptr @glb, align 4
  ret i8 poison
}
