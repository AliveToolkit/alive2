; TEST-ARGS: -tgt-is-asm

define ptr @src1(ptr nonnull %0) {
  %2 = freeze ptr %0
  ret ptr %2
}

define ptr @tgt1(ptr nonnull %0) {
  ret ptr %0
}

define ptr @src2(ptr nonnull %0) {
  %2 = freeze ptr %0
  ret ptr %2
}

define nonnull ptr @tgt2(ptr nonnull %0) {
  ret ptr %0
}
