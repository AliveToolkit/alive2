; TEST-ARGS: --disable-undef-input --disable-poison-input

define i1 @or_ne_ne(i8 %0) {
  %2 = icmp ne i8 %0, 13
  %3 = icmp ne i8 %0, 17
  %4 = or i1 %2, %3
  ret i1 %4
}
