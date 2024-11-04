; this test must use separate files

@x = private constant i32 1

define i32 @f() {
  ret i32 0

dead:    
  call void null(ptr @x)
  unreachable
}
