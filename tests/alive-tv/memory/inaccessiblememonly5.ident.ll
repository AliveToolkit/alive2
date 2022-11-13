define i8 @src() {
  %x = alloca i8
  call void @bar()
  call ptr @foo()
  ret i8 0
}

declare ptr @foo()
declare void @bar() memory(inaccessiblemem: readwrite)
