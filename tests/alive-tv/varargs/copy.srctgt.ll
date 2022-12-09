%struct.va_list = type { ptr }

define i32 @src(...) {
  %ap = alloca %struct.va_list
  %aq = alloca ptr

  call void @llvm.va_start(ptr %ap)

  call void @llvm.va_copy(ptr %aq, ptr %ap)

  %a = va_arg ptr %ap, i32
  call void @llvm.va_end(ptr %ap)

  %b = va_arg ptr %aq, i32
  call void @llvm.va_end(ptr %aq)

  %r = add i32 %a, %b
  ret i32 %r
}

define i32 @tgt(...) {
  %ap = alloca %struct.va_list
  call void @llvm.va_start(ptr %ap)
  %a = va_arg ptr %ap, i32
  call void @llvm.va_end(ptr %ap)
  %r = add i32 %a, %a
  ret i32 %r
}

declare void @llvm.va_start(ptr)
declare void @llvm.va_copy(ptr, ptr)
declare void @llvm.va_end(ptr)
