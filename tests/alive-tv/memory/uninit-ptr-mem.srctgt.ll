define i32* @src() {
  %retval = alloca i32*, align 8
  %v = load i32*, i32** %retval, align 8
  ret i32* %v
}

define i32* @tgt() {
  ret i32* undef
}
