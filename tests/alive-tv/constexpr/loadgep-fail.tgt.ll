@g = constant i16 1

define i8 @f() {
  %1 = getelementptr inbounds i8, ptr @g, i64 0
  %x = load i8, ptr %1, align 1
  ret i8 %x
}
