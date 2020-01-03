@x = global i8* null

define void @f1(i8* nocapture %p) {
  store i8* %p, i8** @x
  ret void
}

define void @f2(i8* nocapture %p0) {
  %p = getelementptr i8, i8* %p0, i32 1
  store i8* %p, i8** @x
  ret void
}

define i8* @f3(i8* nocapture %p) {
  ret i8* %p
}

define i8* @f4(i8* nocapture %p) {
  %p2 = getelementptr i8, i8* %p, i32 1
  ret i8* %p2
}

define <2 x i8*> @f5(i8* nocapture %p) {
  %v = insertelement <2 x i8*> undef, i8* %p, i32 1
  ret <2 x i8*> %v
}

define i8* @f6(i8* nocapture %p, i8* %q) {
  %c = icmp eq i8* %p, %q
  br i1 %c, label %A, label %B
A:
  ret i8* %p
B:
  ret i8* null
}
