; TEST-ARGS: -disable-undef-input
define i8* @f_observed(i64 %size) {
entry:
  %items.addr = alloca i32, align 4
  %call = call noalias i8* @malloc(i64 %size)
  %i = ptrtoint i8* %call to i64
  ret i8* %call
}

; ERROR: Value mismatch
declare dso_local noalias i8* @malloc(i64)
