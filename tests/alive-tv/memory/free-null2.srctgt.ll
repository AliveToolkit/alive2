define void @src(i8* %p) {
  %c = icmp ne i8* %p, null
  br i1 %c, label %A, label %B
A:
  call void @free(i8* %p)
  br label %B
B:
  ret void
}

define void @tgt(i8* %p) {
  call void @free(i8* %p)
  ret void
}

declare void @free(i8*)
