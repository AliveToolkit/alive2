define void @src(i8* %p) {
  %c = icmp eq i8* %p, null
  br i1 %c, label %then, label %else

then:
  br label %exit

else:
  br label %exit

exit:
  call void @f(i8* %p)
  ret void
}

define void @tgt(i8* %p) {
  %c = icmp eq i8* %p, null
  br i1 %c, label %then, label %else

then:
  call void @f(i8* null)
  ret void

else:
  call void @f(i8* %p)
  ret void
}

declare void @f(i8*)
