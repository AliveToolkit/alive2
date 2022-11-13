define i8 @f1() {
  %a = alloca i8
  store i8 3, ptr %a
  %b = call i8 @g(ptr %a)
  ret i8 %b
}

define i8 @f2(ptr %a) {
  %b = call i8 @g(ptr %a)
  ret i8 %b
}

@glb = constant i8 0
define i8 @f2glb() {
  %b = call i8 @g(ptr @glb)
  ret i8 %b
}

define i8 @f3(ptr %a) {
  store i8 3, ptr %a
  %b = call i8 @g(ptr %a)
  ret i8 %b
}

define i8 @f4(ptr %a) {
  %b = call i8 @h(ptr %a)
  store i8 3, ptr %a
  ret i8 %b
}

define i8 @f5() {
  %p = alloca i8
  %b = call i8 @g(ptr %p)
  ret i8 %b
}

define i8 @f6(ptr byval(i8) %p) {
  %b = call i8 @g(ptr %p)
  ret i8 %b
}

define i8 @f6_2(ptr %p) {
  %b = call i8 @g2(ptr byval(i8) %p)
  ret i8 %b
}

define i8 @f6_3() {
  %b = call i8 @g2(ptr byval(i8) @glb)
  ret i8 %b
}

define void @f7() {
  %ptr = call ptr @k()
  %a = load i8, ptr %ptr, align 4
  ret void
}

define void @f8() {
  %call = call ptr @k()
  %x = load i8, ptr %call, align 1
  ret void
}

define void @f9() {
  call void @j(i32 3)
  call void @j(i32 4)
  ret void
}

declare i8 @g(ptr)
declare i8 @g2(ptr byval(i8))
declare i8 @h(ptr) memory(none)
declare void @j(i32)
declare ptr @k()

declare void @llvm.memcpy.p0i8.p0i8.i64(ptr, ptr, i64, i1)
