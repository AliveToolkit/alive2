define i64 @f() {
  %p2 = getelementptr inbounds i32, ptr null, i8 1
  %v = ptrtoint ptr %p2 to i64
  %v2 = add i64 %v, 3
  ret i64 %v2
}
