define void @src(i1 %c) nofree {
  %p = call ptr @malloc(i64 4)
  br i1 %c, label %A, label %EXIT
A:
  call void @free(ptr %p)
  br label %EXIT
EXIT:
  ret void
}

define void @tgt(i1 %c) nofree {
  %p = call ptr @malloc(i64 4)
  call void @free(ptr %p)
  ret void
}

declare void @free(ptr)
declare ptr @malloc(i64)
