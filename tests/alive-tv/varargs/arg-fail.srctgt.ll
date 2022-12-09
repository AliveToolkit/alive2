; ERROR: Target is more poisonous than source

%struct.va_list = type { ptr }

define i32 @src(ptr %p) {
  %a = va_arg ptr %p, i32
  ret i32 %a
}

define i32 @tgt(ptr %p) {
  %a = va_arg ptr %p, i32
  ret i32 poison
}
