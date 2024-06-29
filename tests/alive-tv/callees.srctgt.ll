declare i32 @test()
@fptr = internal global ptr @test

define i32 @src() {
 entry:
   %0 = load ptr, ptr @fptr, align 8
   %1 = call i32 %0(), !callees !0
   ret i32 0
}

define i32 @tgt() {
    entry:
       %0 = load ptr, ptr @fptr, align 8
       %1 = call i32 %0(), !callees !0
       ret i32 0
}

!0 = !{ptr @test, ptr @test}