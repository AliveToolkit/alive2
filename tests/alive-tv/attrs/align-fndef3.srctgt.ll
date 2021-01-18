define align 4 i8* @src(i8* %p) {
  load i8, i8* %p, align 4
  %q = getelementptr inbounds i8, i8* %p, i64 1 ; %q isn't 4 bytes aligned
  ret i8* %q
}

define align 4 i8* @tgt(i8* %p) {
  ret i8* poison
}
