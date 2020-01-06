@x = global i8* null

define void @f1(i8* nocapture %p) {
  %poison = getelementptr inbounds i8, i8* null, i64 1
  store i8* %poison, i8** @x
  ret void
}

define void @f2(i8* nocapture %p) {
  %poison = getelementptr inbounds i8, i8* null, i64 1
  store i8* %poison, i8** @x
  ret void
}

define i8* @f3(i8* nocapture %p) {
  %poison = getelementptr inbounds i8, i8* null, i64 1
  ret i8* %poison
}

define i8* @f4(i8* nocapture %p) {
  %poison = getelementptr inbounds i8, i8* null, i64 1
  ret i8* %poison
}

define <2 x i8*> @f5(i8* nocapture %p) {
  %poison = getelementptr inbounds i8, i8* null, i64 1
  %v = insertelement <2 x i8*> undef, i8* %poison, i32 1
  ret <2 x i8*> %v
}

define i8* @f6(i8* nocapture %p, i8* %q) {
  %c = icmp eq i8* %p, %q
  br i1 %c, label %A, label %B
A:
  ret i8* %q
B:
  ret i8* null
}

define i8* @f7(i8* %a, i8* nocapture %b) {
  %v = call i8* @g(i8* %a)
  ret i8* %v
}

declare i8* @g(i8*)
