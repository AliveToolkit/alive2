define void @fn(i32 %code, i1 %c) {
  br i1 %c, label %then, label %else

then:
  call void @exit(i32 0)
  unreachable

else:
  call void @exit(i32 %code)
  unreachable
}

declare void @exit(i32) noreturn
