; TEST-ARGS: -dbg

@x = global i8* null

define i8* @f() {
  %q = load i8*, i8** @x
  ret i8* %q
}
; CHECK: has_null_block: 1
