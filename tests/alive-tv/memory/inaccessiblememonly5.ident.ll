define i8 @src() {
  %x = alloca i8
  call void @bar()
  call i8* @foo()
  ret i8 0
}

declare i8* @foo()
declare void @bar() inaccessiblememonly
