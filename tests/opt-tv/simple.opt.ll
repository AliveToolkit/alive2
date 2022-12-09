; TEST-ARGS: -passes=sroa,instsimplify

define i32 @f(i1 %cond) {
  %p = alloca i32
  br i1 %cond, label %A, label %B
A:
  store i32 1, i32* %p
  br label %EXIT
B:
  store i32 2, i32* %p
  br label %EXIT
EXIT:
  %v = load i32, i32* %p
  ret i32 %v
}
