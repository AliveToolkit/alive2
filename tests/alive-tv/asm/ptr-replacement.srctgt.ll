; TEST-ARGS: -tgt-is-asm

define i8 @src(ptr %0, ptr %1) {
  store i8 0, ptr %0, align 4
  %c = icmp eq ptr %0, %1
  br i1 %c, label %then, label %else

then:
  ret i8 1
else:
  ret i8 0
}

define i8 @tgt(ptr %0, ptr %1) {
  %c = icmp eq ptr %0, %1
  br i1 %c, label %then, label %else

then:
  store i8 0, ptr %1, align 4
  ret i8 1

else:
  store i8 0, ptr %0, align 4
  ret i8 0
}