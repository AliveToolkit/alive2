; ERROR: Parameter attributes not refined

define ptr @src(ptr %p) {
  ret ptr %p
}

define ptr @tgt(ptr nonnull noundef %p) {
  ret ptr %p
}
