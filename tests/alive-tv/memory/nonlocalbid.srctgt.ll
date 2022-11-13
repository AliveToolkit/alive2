declare void @free(ptr)
declare ptr @f(i32) memory(read)

define i8 @src() {
  %p1 = call ptr @f(i32 0)
  %p2 = call ptr @f(i32 1)
  %p3 = call ptr @f(i32 2)
  %c1 = icmp eq ptr %p1, null
  %c2 = icmp eq ptr %p2, null
  %c3 = icmp eq ptr %p3, null
  %cmp = or i1 %c1, %c2
  %cmp2 = or i1 %cmp, %c3
  br i1 %cmp2, label %A, label %B
A:
  ret i8 1
B:
  call void @free(ptr %p1)
  call void @free(ptr %p2)
  call void @free(ptr %p3)
  ret i8 2
}

define i8 @tgt() {
  ret i8 1
}

; ERROR: Source and target don't have the same return domain
