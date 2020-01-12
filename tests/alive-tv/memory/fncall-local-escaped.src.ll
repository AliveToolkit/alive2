; TEST-ARGS: -smt-to=10000

@glb = global i8* null

define i8 @foo(i1 %cmp) {
  %a = alloca i8
  %b = alloca i8
  %c = alloca i8
  store i8 42, i8* %c
  br i1 %cmp, label %t, label %f

t:
  store i8* %a, i8** @glb
  br label %end

f:
  store i8* %b, i8** @glb
  br label %end

end:
  call void @fn()
  %load = load i8, i8* %c
  ret i8 %load
}

declare void @fn()
