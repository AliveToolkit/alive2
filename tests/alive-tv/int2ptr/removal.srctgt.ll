@b = global ptr null, align 8

define i32 @src(ptr %c, i1 %c2) {
entry:
  %tobool = icmp ne ptr %c, null
  br i1 %tobool, label %if.then, label %exit

if.then:
  br label %exit

exit:
  %d.0 = phi i64 [ 0, %if.then ], [ 1, %entry ]
  inttoptr i64 %d.0 to ptr
  ret i32 0
}

define i32 @tgt(ptr %c, i1 %c2) {
  ret i32 0
}
