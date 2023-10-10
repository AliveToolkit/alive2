target datalayout = "i24:32:32" ; 4-bytes aligned

define ptr @f(ptr %p) {
  %q = getelementptr [4 x i24], ptr %p, i32 0, i32 1
  ret ptr %q
}
