; TEST-ARGS: -smt-to=9000

define i32 @free_ub_alloca() {
  %1 = alloca i32
  %2 = bitcast i32* %1 to i8*
  call void @free(i8* %2)
  ret i32 0
}

declare void @free(i8*)
