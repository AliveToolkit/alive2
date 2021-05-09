define i8* @src() {
  %call = call align 16 i8* @malloc(i64 8)
  store i8 0, i8* %call, align 1
  ret i8* %call
}

define i8* @tgt() {
  %call = call align 16 i8* @malloc(i64 8)
  store i8 0, i8* %call, align 16
  ret i8* %call
}

declare i8* @malloc(i64)
