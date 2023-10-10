define ptr @src() {
  %call = call ptr @malloc(i64 8)
  store i8 0, ptr %call, align 1
  ret ptr %call
}

define ptr @tgt() {
  %call = call ptr @malloc(i64 8)
  store i8 0, ptr %call, align 8
  ret ptr %call
}

declare ptr @malloc(i64)
