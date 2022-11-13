@glb = global ptr null, align 8
@glb2 = global i8 0, align 8

define void @src() {
  %tobool = icmp ne ptr undef, null
  br i1 %tobool, label %if.then, label %if.end

if.then:
  call void @g()
  br label %if.end

if.end:
  %q = load ptr, ptr @glb, align 8
  ret void
}

define void @tgt() {
  %tobool = icmp ne ptr undef, null
  br i1 %tobool, label %if.then, label %if.end

if.then:
  call void @g()
  br label %if.end

if.end:
  ret void
}

declare void @g() memory(write)
