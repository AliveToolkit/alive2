@glb = global ptr null

define i8 @foo(i1 %cmp) {
  %a = alloca i8
  %b = alloca i8
  %c = alloca i8
  store i8 42, ptr %c
  br i1 %cmp, label %t, label %f

t:
  store ptr %a, ptr @glb
  br label %end

f:
  store ptr %b, ptr @glb
  br label %end

end:
  call void @fn()
  %load = load i8, ptr %c
  ret i8 %load
}

declare void @fn()
