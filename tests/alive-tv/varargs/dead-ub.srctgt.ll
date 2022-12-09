%struct.va_list = type { ptr }

define i32 @src(...) {
  %ap = alloca %struct.va_list
  call void @llvm.va_start(ptr %ap)
  call void @llvm.va_end(ptr %ap)
  %a = va_arg ptr %ap, i32
  ret i32 %a
}

define i32 @tgt(...) {
  unreachable
}

declare void @llvm.va_start(ptr)
declare void @llvm.va_end(ptr)
