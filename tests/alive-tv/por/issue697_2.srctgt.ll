@glb = global i8* null, align 8

define void @src(i8* %p) {
  %tobool = icmp ne i8* %p, null
  br i1 %tobool, label %if.then, label %if.end

if.then:
  call void @g(i8* undef)
  br label %if.end

if.end:
  %q = load i8*, i8** @glb, align 8
  ret void
}

define void @tgt(i8* %p) {
  %tobool = icmp ne i8* %p, null
  br i1 %tobool, label %if.then, label %if.end

if.then:
  call void @g(i8* undef)
  br label %if.end

if.end:
  ;%q = load i8*, i8** @glb, align 8
  ret void
}

declare void @g(i8*)
