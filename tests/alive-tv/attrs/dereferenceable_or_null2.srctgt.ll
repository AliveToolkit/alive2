define i32* @src(i32* dereferenceable(4) %p) {
  ret i32* %p
}

define i32* @tgt(i32* dereferenceable_or_null(4) %p) {
  ret i32* %p
}
