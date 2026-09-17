; string attributes are legal on parameters (LLVM-C frontends emit them as a
; fallback when LLVMGetEnumAttributeKindForName returns 0). handleParamAttrs
; used to call getKindAsEnum() on them, which asserts.
define i32 @src(ptr "foo"="bar" %p) {
  %v = load i32, ptr %p
  call void @g(ptr "baz"="qux" %p)
  ret i32 %v
}

define i32 @tgt(ptr "foo"="bar" %p) {
  %v = load i32, ptr %p
  call void @g(ptr "baz"="qux" %p)
  ret i32 %v
}

declare void @g(ptr)
