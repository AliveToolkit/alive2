define void @f(i64* %src) {
  %data = load i64, i64* %src, align 8
  ; force 2 poison bits at least
  %vec = bitcast i64 %data to <2 x i32>
  ret void
}
