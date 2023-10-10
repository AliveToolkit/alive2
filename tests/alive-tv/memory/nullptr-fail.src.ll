define i32 @ret_null() {
  %i = ptrtoint ptr null to i32
  ret i32 %i
}

; ERROR: Value mismatch
