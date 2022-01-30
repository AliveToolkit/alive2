; ERROR: Parameter attributes not refined

define i8* @src(i8* %p) {
  ret i8* %p
}

define i8* @tgt(i8* noundef align(4) %p) {
  ret i8* %p
}
