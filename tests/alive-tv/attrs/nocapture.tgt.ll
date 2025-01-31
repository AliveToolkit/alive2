@x = global ptr null

define void @f1(ptr captures(none) %p) {
  %poison = getelementptr inbounds i8, ptr null, i64 1
  store ptr %poison, ptr @x
  ret void
}

define void @f2(ptr captures(none) %p) {
  %poison = getelementptr inbounds i8, ptr null, i64 1
  store ptr %poison, ptr @x
  ret void
}

define ptr @f3(ptr captures(none) %p) {
  %poison = getelementptr inbounds i8, ptr null, i64 1
  ret ptr %poison
}

define ptr @f4(ptr captures(none) %p) {
  %poison = getelementptr inbounds i8, ptr null, i64 1
  ret ptr %poison
}

define <2 x ptr> @f5(ptr captures(none) %p) {
  %poison = getelementptr inbounds i8, ptr null, i64 1
  %v = insertelement <2 x ptr> undef, ptr %poison, i32 1
  ret <2 x ptr> %v
}

define ptr @f6(ptr captures(none) %p, ptr %q) {
  %c = icmp eq ptr %p, %q
  br i1 %c, label %A, label %B
A:
  ret ptr %q
B:
  ret ptr null
}

define ptr @f7(ptr %a, ptr captures(none) %b) {
  %v = call ptr @g(ptr %a)
  ret ptr %v
}

declare ptr @g(ptr)
