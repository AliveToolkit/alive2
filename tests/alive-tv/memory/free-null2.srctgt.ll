define void @src(ptr %ptr) {
  %c = icmp ne ptr %ptr, null
  br i1 %c, label %then, label %else

then:
  call void @free(ptr %ptr)
  br label %else

else:
  ret void
}

define void @tgt(ptr %ptr) {
  call void @free(ptr %ptr)
  ret void
}

declare void @free(ptr)
