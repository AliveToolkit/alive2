; TEST-ARGS: -disable-undef-input
define i8* @f(i64 %size) {
entry:
  %items.addr = alloca i32, align 4
  %call = call noalias i8* @malloc(i64 %size)
  ret i8* %call
}

define i8* @f_observed(i64 %size) {
entry:
  %items.addr = alloca i32, align 8
  %call = call noalias i8* @malloc(i64 %size)
  %unused = ptrtoint i8* %call to i64
  ret i8* %call
}

declare dso_local noalias i8* @malloc(i64)
