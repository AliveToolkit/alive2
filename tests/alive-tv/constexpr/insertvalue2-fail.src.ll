define {{i8, i8}, {i8, i8}} @func() {
  ; {{1, 2}, {5, 6}}
  %1 = insertvalue {{i8, i8}, {i8, i8}} {{i8, i8} {i8 1, i8 2}, {i8, i8} {i8 5, i8 6}}, i8 8, 0, 0
  ; {{8, 2}, {5, 6}}
  %2 = insertvalue {{i8, i8}, {i8, i8}} %1, i8 10, 0, 1
  ; {{8, 10}, {5, 6}}
  ret {{i8, i8}, {i8, i8}} %2
}

; ERROR: Value mismatch