@b = global i32* null, align 8

define i32 @src(i32* %c, i1 %c2) {
entry:
  %tobool = icmp ne i32* %c, null
  br i1 %tobool, label %if.then, label %exit

if.then:
  br label %exit

exit:
  %d.0 = phi i64 [ 0, %if.then ], [ 1, %entry ]
  inttoptr i64 %d.0 to i32*
  ret i32 0
}

define i32 @tgt(i32* %c, i1 %c2) {
  ret i32 0
}
