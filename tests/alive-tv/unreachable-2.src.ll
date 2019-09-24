define i32 @f() {
  br label %bb
bb:
  unreachable
exit:
  ret i32 1
}
