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
