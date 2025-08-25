define ptr @src() {
  %p = alloca [4 x i8], align 1
  %q2 = freeze ptr poison
  ret ptr %q2
}


@gv1 = private constant i32 51

define ptr @tgt() {
  %1 = load i32, ptr @gv1, align 1
  br label %originalBB

originalBB:
  %p = alloca [4 x i8], align 1
  %q = getelementptr inbounds [4 x i8], ptr %p, i32 0, i32 %1
  %q2 = freeze ptr %q
  ret ptr %q2
}
