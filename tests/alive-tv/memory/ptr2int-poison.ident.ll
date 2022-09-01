define i8 @fn() {
  %i = ptrtoint ptr poison to i8
  ret i8 %i
}
