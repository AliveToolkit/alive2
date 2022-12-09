@glb = global ptr null, align 8

define void @src(ptr %p) {
  %tobool = icmp ne ptr %p, null
  br i1 %tobool, label %if.then, label %if.end

if.then:
  call void @g(ptr undef)
  br label %if.end

if.end:
  %q = load ptr, ptr @glb, align 8
  ret void
}

define void @tgt(ptr %p) {
  %tobool = icmp ne ptr %p, null
  br i1 %tobool, label %if.then, label %if.end

if.then:
  call void @g(ptr undef)
  br label %if.end

if.end:
  ret void
}

declare void @g(ptr)
