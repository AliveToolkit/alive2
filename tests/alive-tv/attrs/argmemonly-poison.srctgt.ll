; ERROR: Source is more defined than target

@glb = external global i8

define void @src(ptr %p) memory(argmem: readwrite) {
  load i8, ptr %p
  %c = icmp eq ptr %p, @glb
  br i1 %c, label %true, label %false

true:
  br label %end

false:
  call void @f(ptr @glb)
  br label %end

end:
  ret void
}

define void @tgt(ptr %p) memory(argmem: readwrite) {
  %c = icmp eq ptr %p, @glb
  br i1 %c, label %true, label %false

true:
  br label %end

false:
  call void @f(ptr poison)
  br label %end

end:
  ret void
}

declare void @f(ptr) memory(argmem: readwrite)
