declare i8 @fn()
@fptr = global ptr null, align 8

define i8 @src() {
  %f = load ptr, ptr @fptr, align 8
  %c = call i8 %f()
  ret i8 %c
}

define i8 @tgt() {
  %f = load ptr, ptr @fptr, align 8
  %cmp = icmp eq ptr %f, @fn
  br i1 %cmp, label %fn, label %else

fn:
  %c = call i8 @fn()
  ret i8 %c

else:
  %call = call i8 %f()
  ret i8 %call
}
