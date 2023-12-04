define ptr @src() {
  %call = call nonnull ptr @_Znwm(i64 8)
  call void @fn(ptr %call) memory(argmem:write)
  ret ptr %call
}

define ptr @tgt() {
  %call = call nonnull ptr @_Znwm(i64 8)
  call void @fn(ptr %call) memory(argmem:write)
  ret ptr %call
}

declare ptr @_Znwm(i64)
declare void @fn(ptr)
