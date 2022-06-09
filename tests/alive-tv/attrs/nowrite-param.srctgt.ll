@A = external global i32
@B = external global i32

define i32 @src(i1 %z) {
  %l = call i32 @f_load_arg(ptr @A)
  call void @f_readonly_arg(ptr @A, ptr @B)
  br i1 %z, label %true, label %false

true:
  ret i32 %l

false:
  ret i32 0
}

define i32 @tgt(i1 %z) {
  call void @f_readonly_arg(ptr @A, ptr @B)
  br i1 %z, label %true, label %false
true:
  %l = call i32 @f_load_arg(ptr @A)
  ret i32 %l

false:
  ret i32 0
}

declare i32 @f_load_arg(i32*) nounwind willreturn readonly argmemonly
declare void @f_readonly_arg(i32* readonly, i32*) nounwind willreturn argmemonly
