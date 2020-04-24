; TEST-ARGS: -disable-undef-input

define i1 @src(i8* %a, i8* %b, i64 %ofs) {
  %a2 = getelementptr i8, i8* %a, i64 %ofs
  %b2 = getelementptr i8, i8* %b, i64 %ofs
  %c = icmp eq i8* %a2, %b2
  ret i1 %c
}

define i1 @tgt(i8* %a, i8* %b, i64 %ofs) {
  %c = icmp eq i8* %a, %b
  ret i1 %c
}
