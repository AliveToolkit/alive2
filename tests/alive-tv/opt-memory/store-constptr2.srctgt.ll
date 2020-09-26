; TEST-ARGS: -dbg
@x = global i8* undef
@y = global i8* undef

define void @src(i8* %a) {
  store i8* %a, i8** @y
  store i8* null, i8** @x
  ret void
}

define void @tgt(i8* %a) {
  store i8* %a, i8** @y
  store i8* null, i8** @x
  ret void
}

; CHECK: does_ptr_store_nonconstant: 1

