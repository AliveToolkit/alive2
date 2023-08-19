@glb = global ptr null
declare ptr @f(ptr %p)

define i8 @src() {
  %p = alloca i8
  store i8 1, ptr %p

  %q = load ptr, ptr @glb
  %q2 = call ptr @f(ptr %q) ; cannot be %p
  store i8 2, ptr %q2

  %v = load i8, ptr %p
  ret i8 %v
}

define i8 @tgt() {
  %p = alloca i8
  store i8 1, ptr %p

  %q = load ptr, ptr @glb
  %q2 = call ptr @f(ptr %q) ; cannot be %p
  store i8 2, ptr %q2

  ret i8 1
}
