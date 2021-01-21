; TEST-ARGS: -dbg
; CHECK: num_nonlocals_src: 6

define void @f(i8** %p, i1 %c) {
  br i1 %c, label %then, label %else

then:
  load i8*, i8** %p
  %p01 = getelementptr i8*, i8** %p, i32 3
  load i8*, i8** %p01
  br label %exit

else:
  load i8*, i8** %p
  %p2 = getelementptr i8*, i8** %p, i32 1
  load i8*, i8** %p2
  %p3 = getelementptr i8*, i8** %p, i32 2
  load i8*, i8** %p3
  %p4 = getelementptr i8*, i8** %p, i32 4
  load i8*, i8** %p4
  br label %exit

exit:
  load i8*, i8** %p
  ret void
}
