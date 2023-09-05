define void @src(ptr %p) {
  %c = icmp eq ptr %p, null
  br i1 %c, label %then, label %else

then:
  br label %exit

else:
  br label %exit

exit:
  call void @f(ptr %p)
  ret void
}

define void @tgt(ptr %p) {
  %c = icmp eq ptr %p, null
  br i1 %c, label %then, label %else

then:
  call void @f(ptr null)
  ret void

else:
  call void @f(ptr %p)
  ret void
}

declare void @f(ptr)
