define void @f(ptr %src) {
  %data = load i64, ptr %src, align 8
  ; force 2 poison bits at least
  %vec = bitcast i64 %data to <2 x i32>
  ret void
}
