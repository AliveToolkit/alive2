define i8 @src() {
  %x = alloca ptr, align 8
  store ptr @fn, ptr %x, align 8
  %f = load ptr, ptr %x, align 8
  %call = call i8 %f()
  ret i8 %call
}

define i8 @tgt() {
  %call = call i8 @fn()
  ret i8 %call
}

declare i8 @fn()
