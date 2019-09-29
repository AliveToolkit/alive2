define i64 @f() {
  %p2 = getelementptr inbounds i32, i32* null, i8 0
  %v = ptrtoint i32* %p2 to i64
  %v2 = add i64 %v, 3
  ret i64 %v2
}

; ERROR: Value mismatch
