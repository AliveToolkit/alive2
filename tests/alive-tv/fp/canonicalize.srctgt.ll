define i1 @src() {
  %a = bitcast i32 2139095042 to float
  %b = bitcast i32 2139095044 to float
  %a2 = call float @llvm.canonicalize.f32(float %a)
  %b2 = call float @llvm.canonicalize.f32(float %b)
  %x = bitcast float %a2 to i32
  %y = bitcast float %b2 to i32
  %cmp = icmp eq i32 %x, %y
  ret i1 %cmp
}

define i1 @tgt() {
  ret i1 true
}

declare float @llvm.canonicalize.f32(float)
