define void @src(i8* %ptr) {
  %c = icmp ne i8* %ptr, null
  br i1 %c, label %then, label %else

then:
  call void @free(i8* %ptr)
  br label %else

else:
  ret void
}

define void @tgt(i8* %ptr) {
  call void @free(i8* %ptr)
  ret void
}

declare void @free(i8*)
