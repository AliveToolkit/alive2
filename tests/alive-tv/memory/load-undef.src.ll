@gv = global i8 0

define void @fn(i8 %v) {
entry:
  %v.addr = alloca i8
  store i8 %v, ptr %v.addr
  %0 = load i8, ptr %v.addr

  %cmp = icmp eq i8 %0, 1
  br i1 %cmp, label %if.then, label %if.end

if.then:
  %1 = load i8, ptr %v.addr
  store i8 %1, ptr @gv
  br label %if.end

if.end:
  ret void
}


define i8 @fn2() {
entry:
  %v.addr = alloca i8
  store i8 undef, ptr %v.addr

  %0 = load i8, ptr %v.addr
  %1 = load i8, ptr %v.addr
  %2 = add i8 %0, %1
  ret i8 %2
}
