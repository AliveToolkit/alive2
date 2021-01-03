%struct.va_list = type { i8* }

define i32 @src(...) {
  %ap = alloca %struct.va_list
  %aq = alloca i8*

  %ap2 = bitcast %struct.va_list* %ap to i8*
  call void @llvm.va_start(i8* %ap2)

  %aq2 = bitcast i8** %aq to i8*
  call void @llvm.va_copy(i8* %aq2, i8* %ap2)

  %a = va_arg i8* %ap2, i32
  call void @llvm.va_end(i8* %ap2)

  %b = va_arg i8* %aq2, i32
  call void @llvm.va_end(i8* %aq2)

  %r = add i32 %a, %b
  ret i32 %r
}

define i32 @tgt(...) {
  %ap = alloca %struct.va_list
  %ap2 = bitcast %struct.va_list* %ap to i8*
  call void @llvm.va_start(i8* %ap2)
  %a = va_arg i8* %ap2, i32
  call void @llvm.va_end(i8* %ap2)
  %r = add i32 %a, %a
  ret i32 %r
}

declare void @llvm.va_start(i8*)
declare void @llvm.va_copy(i8*, i8*)
declare void @llvm.va_end(i8*)
