; TEST-ARGS: -dbg

@x = global i8* null

define i8* @src() {
  %q = load i8*, i8** @x
  ret i8* %q
}

define i8* @tgt() {
  %q = load i8*, i8** @x
  ret i8* %q
}

; CHECK: has_null_block: 1
