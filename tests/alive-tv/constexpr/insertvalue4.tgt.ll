define {{i8, i8}, {i8, i8}} @func() {
  ret { {i8 ,i8}, {i8, i8}} { {i8, i8} undef, {i8, i8} {i8 8, i8 10}}
}
