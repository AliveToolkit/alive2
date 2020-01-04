; ERROR: Source is more defined than target

@x = global i8* null

define void @f(i8* nocapture %p) {
  %poison = getelementptr inbounds i8, i8* null, i32 1
  store i8* %poison, i8** @x
  ret void
}
