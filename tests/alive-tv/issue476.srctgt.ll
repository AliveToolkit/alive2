; ERROR: Parameter attributes not refined

define i32 @src(i8* dereferenceable(4) %p) {
  ret i32 0
}

define i32 @tgt(i8* dereferenceable(8) %p) {
  ret i32 0
}
