@glb = external global i8

define void @src(i8 *%p) argmemonly {
  load i8, i8* %p
  %c = icmp eq i8* %p, @glb
  br i1 %c, label %true, label %false

true:
  br label %end

false:
  call void @f(i8* @glb)
  br label %end

end:
  ret void
}

define void @tgt(i8 *%p) argmemonly {
  %c = icmp eq i8* %p, @glb
  br i1 %c, label %true, label %false

true:
  br label %end

false:
  call void @f(i8* poison)
  br label %end

end:
  ret void
}

declare void @f(i8*) argmemonly
