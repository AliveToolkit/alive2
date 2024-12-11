; ERROR: Target is more poisonous

define ptr @src() {
  %a = alloca i32
  ret ptr %a
}

define ptr @tgt() {
  ret ptr poison
}
