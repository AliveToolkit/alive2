; Found by Alive2

define i8* @src(i8* %p, i8* %q) {
  %i = ptrtoint i8* %p to i64
  %j = ptrtoint i8* %q to i64
  %diff = sub i64 %j, %i
  %p2 = getelementptr i8, i8* %p, i64 %diff
  ret i8* %p2
}

define i8* @tgt(i8* %p, i8* %q) {
  ret i8* %q
}

; ERROR: Value mismatch
