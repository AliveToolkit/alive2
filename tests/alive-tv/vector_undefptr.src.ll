define <2 x i8*> @f() {
  %a = getelementptr i8, i8* undef, <2 x i64> <i64 0, i64 0>
  ret <2 x i8*> %a
}
