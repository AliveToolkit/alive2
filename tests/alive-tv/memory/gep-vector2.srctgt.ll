target datalayout = "e-p:64:64-p1:16:16-p2:32:32:32-p3:64:64:64-f16:32"

@Global = external global [10 x i8]

define void @src() {
  %A = getelementptr <2 x half>, ptr @Global, i64 0, i64 1
  store i8 0, ptr %A
  ret void
}

define void @tgt() {
  %A = getelementptr i8, ptr @Global, i64 2
  store i8 0, ptr %A
  ret void
}
