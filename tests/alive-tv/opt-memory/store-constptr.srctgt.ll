; TEST-ARGS: -dbg
@x = global i8* undef

define void @src() {
  store i8* null, i8** @x
  ret void
}

define void @tgt() {
  store i8* null, i8** @x
  ret void
}

; CHECK: does_ptr_store_constantsonly: 1

