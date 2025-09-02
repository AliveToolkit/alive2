define i16 @src1(i1 %flag) {
  %a = alloca i16
  %b = alloca i16
  store i16 0, ptr %a
  store i16 1, ptr %b
  br i1 %flag, label %if.then, label %if.else

if.then:
  br label %if.end

if.else:
  br label %if.end

if.end:
  %tmp = phi ptr [ %b, %if.then ], [ %a, %if.else ]
  %result = load i16, ptr %tmp
  ret i16 %result
}
define i16 @tgt1(i1 %flag) {
  %r = zext i1 %flag to i16
  ret i16 %r
}


define i16 @src2(i1 %flag) {
  %a = alloca i16, align 2
  %b = alloca i8, align 2
  store i16 0, ptr %a, align 2
  store i16 1, ptr %b, align 2
  br i1 %flag, label %if.then, label %if.else

if.then:
  br label %if.end

if.else:
  br label %if.end

if.end:
  %tmp = phi ptr [ %b, %if.then ], [ %a, %if.else ]
  %result = load i16, ptr %tmp, align 2
  ret i16 %result
}
define i16 @tgt2(i1 %flag) {
  %r = zext i1 %flag to i16
  ret i16 %r
}


define i16 @src3(i1 %flag) {
  %a = alloca i16, align 2
  %b = alloca i8, align 2
  store i16 0, ptr %a, align 2
  store i8 1, ptr %b, align 2
  br i1 %flag, label %if.then, label %if.else

if.then:
  br label %if.end

if.else:
  br label %if.end

if.end:
  %tmp = phi ptr [ %b, %if.then ], [ %a, %if.else ]
  %result = load i16, ptr %tmp, align 2
  ret i16 %result
}
define i16 @tgt3(i1 %flag) {
  %r = zext i1 %flag to i16
  ret i16 %r
}
