define void @src(ptr %0, i1 %1) {
  %3 = alloca i64, align 8
  store ptr null, ptr %0, align 8
  call void @fn()
  br i1 %1, label %4, label %5

4:
  ret void

5:
  store i64 0, ptr %3, align 8
  ret void
}

define void @tgt(ptr %0, i1 %1) {
  store ptr null, ptr %0, align 8
  call void @fn()
  ret void
}

declare void @fn()
