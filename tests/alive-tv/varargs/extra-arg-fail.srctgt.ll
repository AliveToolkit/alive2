; ERROR: Source is more defined than target

%struct.va_list = type { ptr }

define i32 @src(...) {
  %ap = alloca %struct.va_list
  call void @llvm.va_start(ptr %ap)
  %a = va_arg ptr %ap, i32
  call void @llvm.va_end(ptr %ap)
  ret i32 %a
}

define i32 @tgt(...) {
  %ap = alloca %struct.va_list
  call void @llvm.va_start(ptr %ap)
  %a = va_arg ptr %ap, i32
  %b = va_arg ptr %ap, i32
  call void @llvm.va_end(ptr %ap)
  ret i32 %a
}

declare void @llvm.va_start(ptr)
declare void @llvm.va_end(ptr)
