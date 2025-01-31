define i8 @src(ptr captures(none) %0) {
  %2 = call i8 %0(ptr null)
  ret i8 %2
}

define i8 @tgt(ptr captures(none) %0) {
  %2 = icmp eq ptr %0, @_Z3fooPi
  br i1 %2, label %then, label %else

else:
  %3 = call i8 %0(ptr null)
  br label %ret

then:
  %4 = call i8 @_Z3fooPi(ptr null)
  br label %ret

ret:
  %5 = phi i8 [ %3, %else ], [ %4, %then ]
  ret i8 %5
}

declare i8 @_Z3fooPi(ptr)
