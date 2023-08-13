target datalayout = "e-p:32:32"
@str = constant [3 x i8] zeroinitializer
@str2 = constant [4 x i8] zeroinitializer

define i32 @src(i32 %v0_init) {
entry:
  %v0 = alloca i32, align 4
  %tmp = getelementptr [3 x i8], ptr @str, i32 0, i32 0
  store i32 %v0_init, ptr %v0
  %tmp1 = load i32, ptr %v0
  %ovm = and i32 %tmp1, 32
  %ov3 = add i32 %ovm, 145
  %ov110 = xor i32 %ov3, 153
  %hvar174 = add i32 %ov110, 1
  %tmp2 = getelementptr [4 x i8], ptr @str2, i32 0, i32 0
  call void (ptr, ...) @myprintf(ptr %tmp2, i32 %hvar174)
  br label %return
return:
  ret i32 0
}

define i32 @tgt(i32 %v0_init) {
entry:
  %v0 = alloca i32, align 4
  %tmp = getelementptr [3 x i8], ptr @str, i32 0, i32 0
  store i32 %v0_init, ptr %v0
  %tmp1 = load i32, ptr %v0
  %ovm = and i32 %tmp1, 32
  %ov110 = or i32 %ovm, 9
  %hvar174 = add i32 %ov110, 1
  %tmp2 = getelementptr [4 x i8], ptr @str2, i32 0, i32 0
  call void (ptr, ...) @myprintf(ptr %tmp2, i32 %hvar174)
  br label %return
return:
  ret i32 0

}

declare void @myprintf(ptr, ...)

; ERROR: Source is more defined than target
