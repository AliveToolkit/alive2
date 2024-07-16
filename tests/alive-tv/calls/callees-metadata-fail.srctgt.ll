; ERROR: Source is more defined than target

declare i8 @test()
declare i8 @test2()
@fptr = external global ptr

define i8 @src() {
  %p = load ptr, ptr @fptr, align 8
  %ret = call i8 %p(), !callees !0
  ret i8 %ret
}

define i8 @tgt() {
  %ret = call i8 @test()
  ret i8 %ret
}

!0 = !{ptr @test, ptr @test2}
