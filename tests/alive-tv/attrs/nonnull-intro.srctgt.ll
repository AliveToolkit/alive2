; ERROR: Target is more poisonous than source

define ptr @src(ptr %p) {
  ret ptr %p
}

define ptr @tgt(ptr nonnull %p) {
  ret ptr %p
}
