@glb = global ptr null

define i8 @foo(i1 %cmp) {
  %a = alloca i8
  %b = alloca i8
  %c = alloca i8
  %a.b = select i1 %cmp, ptr %a, ptr %b
  store ptr %a.b, ptr @glb
  call void @fn()
  ret i8 42
}

declare void @fn()
