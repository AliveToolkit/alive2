; An optimization that is done by GVN
define i32 @src1(i32 returned %x) {
  %c = icmp eq i32 %x, 10
  br i1 %c, label %A, label %B
A:
  ret i32 %x
B:
  ret i32 0
}

define i32 @tgt1(i32 returned %x) {
  %c = icmp eq i32 %x, 10
  br i1 %c, label %A, label %B
A:
  ret i32 10
B:
  ret i32 0
}


define i32 @src2(i32 returned %x) {
  %c = icmp eq i32 %x, 10
  br i1 %c, label %A, label %B
A:
  ret i32 11 ; UB
B:
  ret i32 0
}

define i32 @tgt2(i32 returned %x) {
  %c = icmp eq i32 %x, 10
  br i1 %c, label %A, label %B
A:
  unreachable
B:
  ret i32 0
}


define <2 x i4> @src3(<2 x i4> %0) {
  ret <2 x i4> %0
}

define <2 x i4> @tgt3(<2 x i4> returned %0) {
  ret <2 x i4> %0
}


define nonnull ptr @src4(ptr %0) {
  ret ptr %0
}

define nonnull ptr @tgt4(ptr returned %0) {
  ret ptr %0
}
