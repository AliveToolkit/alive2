; TEST-ARGS: -disable-poison-input-allowance
; EXPECT: ERROR: Source is more defined than target

; Should return posioned value based on LangRef Change https://github.com/llvm/llvm-project/pull/157852
; Pulled this code from nunoplopes https://github.com/AliveToolkit/alive2/issues/1242

define i8 @src() {
  %p = alloca i8
  call void @llvm.lifetime.start.p0(ptr %p)
  call void @llvm.lifetime.end.p0(ptr %p)
  %v = load i8, ptr %p
  ret i8 %v
}

define i8 @tgt() {
  unreachable
}
