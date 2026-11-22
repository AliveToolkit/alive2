; The fixed-size struct stores four bytes and has an eight-byte allocation
; size. Its array stride must include the allocation padding only once.

target datalayout = "e-p:64:64-a:64"
%pair = type { <2 x i8>, <2 x i8> }

define <2 x i8> @src_array(ptr %p) {
  %a = load [2 x %pair], ptr %p, align 1
  %r = extractvalue [2 x %pair] %a, 1, 0
  ret <2 x i8> %r
}

define <2 x i8> @tgt_array(ptr %p) {
  %q = getelementptr i8, ptr %p, i64 8
  %r = load <2 x i8>, ptr %q, align 1
  ret <2 x i8> %r
}

define i8 @src_store() {
  ret i8 42
}

define i8 @tgt_store() {
  %p = alloca [8 x i8], align 8
  %q = getelementptr i8, ptr %p, i64 4
  store i8 42, ptr %q, align 1
  store %pair zeroinitializer, ptr %p, align 1
  %r = load i8, ptr %q, align 1
  ret i8 %r
}
