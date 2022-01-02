; ERROR: Source is more defined than target

@x = constant i8 0
@y = constant i8 1

declare i8 @f(i8* %p) readonly argmemonly

define i8 @src() {
  %r = call i8 @f(i8* @x)
  ret i8 %r
}

define i8 @tgt() {
  %r = call i8 @f(i8* @y)
  ret i8 %r
}
