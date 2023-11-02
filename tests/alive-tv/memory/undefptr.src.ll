define void @f() {
  %a = bitcast ptr undef to ptr
  %b = getelementptr i32, ptr %a, i32 1
  store i32 undef, ptr %b
  ret void
}
