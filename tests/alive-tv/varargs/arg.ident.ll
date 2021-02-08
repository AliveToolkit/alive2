%struct.va_list = type { i8* }

define i32 @f(%struct.va_list* %p) {
  %p2 = bitcast %struct.va_list* %p to i8*
  %a = va_arg i8* %p2, i32
  ret i32 %a
}
