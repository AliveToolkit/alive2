; Found by Alive2
target datalayout = "e-i64:64-v16:16-v32:32-n16:32:64-p:64:64:64-p1:32:32:32"

define void @src(ptr %input, i64 %s, i64 %t) {
  %p0 = getelementptr inbounds [10 x [5 x i32]], ptr %input, i64 0, i64 %s, i64 %t
  call void @foo(ptr %p0)

  %s2 = shl nsw i64 %s, 1
  %p1 = getelementptr inbounds [10 x [5 x i32]], ptr %input, i64 0, i64 %s2, i64 %t
  call void @foo(ptr %p1)

  ret void
}

define void @tgt(ptr %input, i64 %s, i64 %t) {
  %p0 = getelementptr inbounds [10 x [5 x i32]], ptr %input, i64 0, i64 %s, i64 %t
  call void @foo(ptr %p0)
  %1 = mul i64 %s, 5
  %p1 = getelementptr inbounds i32, ptr %p0, i64 %1
  call void @foo(ptr %p1)
  ret void
}
declare void @foo(ptr)

; ERROR: Source is more defined than target
