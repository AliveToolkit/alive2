; TEST-ARGS: --uf-float

define i32 @src(float noundef %x) {
  %a = bitcast float %x to i32
  ret i32 %a
}

define i32 @tgt(float noundef %x) {
  %a = alloca float
  store float %x, ptr %a
  %b = load i32, ptr %a
  ret i32 %b
}