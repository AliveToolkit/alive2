define ptr @src(ptr %in) {
  %c = icmp eq ptr %in, null
  %r = select i1 %c, ptr null, ptr %in
  ret ptr %r
}

define ptr @tgt(ptr %in) {
  ret ptr %in
}
