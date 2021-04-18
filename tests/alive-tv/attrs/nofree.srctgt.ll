define void @src(i1 %c) nofree {
  %p = call i8* @malloc(i64 4)
  br i1 %c, label %A, label %EXIT
A:
  call void @free(i8* %p)
  br label %EXIT
EXIT:
  ret void
}

define void @tgt(i1 %c) nofree {
  %p = call i8* @malloc(i64 4)
  call void @free(i8* %p)
  ret void
}

declare void @free(i8*)
declare i8* @malloc(i64)
