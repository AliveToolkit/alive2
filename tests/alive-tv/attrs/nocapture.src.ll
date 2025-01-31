@x = global ptr null

define void @f1(ptr captures(none) %p) {
  store ptr %p, ptr @x
  ret void
}

define void @f2(ptr captures(none) %p0) {
  %p = getelementptr i8, ptr %p0, i32 1
  store ptr %p, ptr @x
  ret void
}

define ptr @f3(ptr captures(none) %p) {
  ret ptr %p
}

define ptr @f4(ptr captures(none) %p) {
  %p2 = getelementptr i8, ptr %p, i32 1
  ret ptr %p2
}

define <2 x ptr> @f5(ptr captures(none) %p) {
  %v = insertelement <2 x ptr> undef, ptr %p, i32 1
  ret <2 x ptr> %v
}

define ptr @f6(ptr captures(none) %p, ptr %q) {
  %c = icmp eq ptr %p, %q
  br i1 %c, label %A, label %B
A:
  ret ptr %p
B:
  ret ptr null
}

define ptr @f7(ptr %a, ptr captures(none) %b) {
  %v = call ptr @g(ptr %a)
  ret ptr %v
}

declare ptr @g(ptr)
