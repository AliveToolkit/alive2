define i32 @src(ptr %0, i1 %1) {
  br i1 %1, label %then, label %else

then:
  ret i32 1

else:
  unreachable
}

define i32 @tgt(ptr %0, i1 %1) {
  %stack = alloca i32
  br i1 %1, label %then, label %else

then:
  store i32 1, ptr %stack
  br label %exit

else:
  store i32 0, ptr %0
  br label %exit

exit:
  %v = load i32, ptr %stack
  ret i32 %v
}
