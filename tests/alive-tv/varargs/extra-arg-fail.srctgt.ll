; ERROR: Source is more defined than target

%struct.va_list = type { i8* }

define i32 @src(...) {
  %ap = alloca %struct.va_list
  %ap2 = bitcast %struct.va_list* %ap to i8*
  call void @llvm.va_start(i8* %ap2)
  %a = va_arg i8* %ap2, i32
  call void @llvm.va_end(i8* %ap2)
  ret i32 %a
}

define i32 @tgt(...) {
  %ap = alloca %struct.va_list
  %ap2 = bitcast %struct.va_list* %ap to i8*
  call void @llvm.va_start(i8* %ap2)
  %a = va_arg i8* %ap2, i32
  %b = va_arg i8* %ap2, i32
  call void @llvm.va_end(i8* %ap2)
  ret i32 %a
}

declare void @llvm.va_start(i8*)
declare void @llvm.va_end(i8*)
