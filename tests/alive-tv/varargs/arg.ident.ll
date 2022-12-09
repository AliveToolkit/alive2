%struct.va_list = type { ptr }

define i32 @f(ptr %p) {
  %a = va_arg ptr %p, i32
  ret i32 %a
}
