define align 4 ptr @src(ptr %p) {
  load i8, ptr %p, align 4
  %q = getelementptr inbounds i8, ptr %p, i64 1 ; %q isn't 4 bytes aligned
  ret ptr %q
}

define align 4 ptr @tgt(ptr %p) {
  ret ptr poison
}
