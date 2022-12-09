define <2 x ptr> @f() {
  %a = getelementptr i8, ptr undef, <2 x i64> <i64 0, i64 0>
  ret <2 x ptr> %a
}
