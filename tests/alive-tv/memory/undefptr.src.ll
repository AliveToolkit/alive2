define void @f() {
  %a = bitcast i8* undef to i32*
  %b = getelementptr i32, i32* %a, i32 1
  store i32 undef, i32* %b
  ret void
}
