define ptr @src() {
  %retval = alloca ptr, align 8
  %v = load ptr, ptr %retval, align 8
  ret ptr %v
}

define ptr @tgt() {
  ret ptr undef
}
