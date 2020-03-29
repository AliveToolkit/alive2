define {{i8, i8}, {i8, i8}} @func() {
  %1 = insertvalue {{i8, i8}, {i8, i8}} undef, i8 8, 1, 0
  %2 = insertvalue {{i8, i8}, {i8, i8}} %1, i8 10, 1, 1
  ret {{i8, i8}, {i8, i8}} %2
}
