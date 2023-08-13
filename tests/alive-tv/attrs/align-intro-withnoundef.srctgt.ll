; ERROR: Parameter attributes not refined

define ptr @src(ptr %p) {
  ret ptr %p
}

define ptr @tgt(ptr noundef align(4) %p) {
  ret ptr %p
}
