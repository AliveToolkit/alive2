define <2 x i8*> @f() {
  %a0 = insertelement <2 x i8*> zeroinitializer, i8* undef, i32 0
  %a = insertelement <2 x i8*> %a0, i8* undef, i32 1
  ret <2 x i8*> %a
}
