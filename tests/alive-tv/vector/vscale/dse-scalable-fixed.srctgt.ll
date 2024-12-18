; SKIP-IDENTITY

define void @src(ptr %ptr) vscale_range(1, 2) {
  %gep.ptr.2 = getelementptr i64, ptr %ptr, i64 2
  store <2 x i64> zeroinitializer, ptr %gep.ptr.2
  store <vscale x 4 x i64> zeroinitializer, ptr %ptr
  ret void
}

define void @tgt(ptr %ptr) vscale_range(1, 2) {
  store <vscale x 4 x i64> zeroinitializer, ptr %ptr
  ret void
}

; ERROR: program doesn't type check!
