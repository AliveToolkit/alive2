; SKIP-IDENTITY

define void @src(ptr %ptr) vscale_range(1, 4) {
  %gep.ptr.8 = getelementptr i64, ptr %ptr, i64 8
  store <vscale x 4 x i64> zeroinitializer, ptr %gep.ptr.8
  store <vscale x 2 x i64> zeroinitializer, ptr %ptr
  ret void
}

define void @tgt(ptr %ptr) vscale_range(1, 4) {
  store <vscale x 2 x i64> zeroinitializer, ptr %ptr
  ret void
}

; ERROR: program doesn't type check!
