declare void @free(i8*)
declare i8* @f(i32) readonly

define i8 @src() {
  %p1 = call i8* @f(i32 0)
  %p2 = call i8* @f(i32 1)
  %p3 = call i8* @f(i32 2)
  %c1 = icmp eq i8* %p1, null
  %c2 = icmp eq i8* %p2, null
  %c3 = icmp eq i8* %p3, null
  %cmp = or i1 %c1, %c2
  %cmp2 = or i1 %cmp, %c3
  br i1 %cmp2, label %A, label %B
A:
  ret i8 1
B:
  call void @free(i8* %p1)
  call void @free(i8* %p2)
  call void @free(i8* %p3)
  ret i8 2
}

define i8 @tgt() {
  ret i8 1
}

; ERROR: Value mismatch
