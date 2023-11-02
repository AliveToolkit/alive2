target datalayout = "i24:32:32" ; 4-bytes aligned

define ptr @f(ptr %p) {
  %q = getelementptr i8, ptr %p, i32 4
  ret ptr %q
}
