; ERROR: Source is more defined than target

define ptr @src(ptr %p) {
  ret ptr %p
}

define ptr @tgt(ptr align(4) %p) {
  ret ptr %p
}
