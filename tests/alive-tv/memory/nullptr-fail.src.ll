define i32 @ret_null() {
  %i = ptrtoint i8* null to i32
  ret i32 %i
}

; ERROR: Value mismatch
