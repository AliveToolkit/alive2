; Found by Alive2

define i32* @src(<7 x i32>* %x, i64 %y, i64 %z) {
  %arr_ptr = bitcast <7 x i32>* %x to [7 x i32]*
  %gep = getelementptr [7 x i32], [7 x i32]* %arr_ptr, i64 %y, i64 %z
  ret i32* %gep
}

define i32* @tgt(<7 x i32>* %x, i64 %y, i64 %z) {
  %gep = getelementptr <7 x i32>, <7 x i32>* %x, i64 %y, i64 %z
  ret i32* %gep
}

; ERROR: Value mismatch
