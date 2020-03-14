; TEST-ARGS: -disable-poison-input -smt-to=10000

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

define i8* @f_observed2(i64 %size) {
entry:
  %items.addr = alloca i32, align 8
  %call = call noalias i8* @calloc(i64 4, i64 %size)
  %unused = ptrtoint i8* %call to i64
  ret i8* %call
}

define i8* @f_observed3(i64 %size) {
entry:
  %unused0 = bitcast i64 %size to i64
  %items.addr = alloca i32, align 8
  %call = call noalias i8* @calloc(i64 4, i64 %size)
  %unused = ptrtoint i8* %call to i64
  ret i8* %call
}

declare noalias i8* @malloc(i64)
declare noalias i8* @calloc(i64, i64)
