@glb = global i8* null

define i8 @foo(i1 %cmp) {
  %a = alloca i8
  %b = alloca i8
  %c = alloca i8
  %a.b = select i1 %cmp, i8* %a, i8* %b
  store i8* %a.b, i8** @glb
  call void @fn()
  ret i8 42
}

declare void @fn()
