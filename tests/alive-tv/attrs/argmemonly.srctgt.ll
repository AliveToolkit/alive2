define i8 @src(i8 *%p) argmemonly {
  %p2 = getelementptr i8, i8* %p, i32 0
  %v = call i8 @f(i8* %p2)
  ret i8 %v
}

define i8 @tgt(i8 *%p) argmemonly {
  %v = call i8 @f(i8* %p)
  ret i8 %v
}

declare i8 @f(i8*) argmemonly
