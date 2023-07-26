; To support this test, escaped local blocks should have bytes updated after
; unkown fn calls.

target datalayout = "e-p:32:32:32-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:32:64-f32:32:32-f64:32:64-v64:64:64-v128:128:128-a0:0:64-f80:128:128-n8:16:32"
target triple = "i386-apple-darwin10"

%struct1 = type { i32, i32 }
%struct2 = type { %struct1, ptr }

define i32 @src() noinline {
  %x = alloca %struct1, align 8
  %y = alloca %struct2, align 8
  call void @bar(ptr sret(%struct1) %x)

  %gepn1 = getelementptr inbounds %struct2, ptr %y, i32 0, i32 0, i32 0
  store i32 0, ptr %gepn1, align 8
  %gepn2 = getelementptr inbounds %struct2, ptr %y, i32 0, i32 0, i32 1
  store i32 0, ptr %gepn2, align 4

  %load = load i64, ptr %x, align 8
  store i64 %load, ptr %y, align 8

  %gep1 = getelementptr %struct2, ptr %y, i32 0, i32 0, i32 0
  %ret = load i32, ptr %gep1
  ret i32 %ret
}

define i32 @tgt() noinline {
  %x = alloca %struct1, align 8
  %y = alloca %struct2, align 8
  %y1 = bitcast ptr %y to ptr
  call void @bar(ptr sret(%struct1) %y1)
  %gepn1 = getelementptr inbounds %struct2, ptr %y, i32 0, i32 0, i32 0
  store i32 0, ptr %gepn1, align 8
  %gepn2 = getelementptr inbounds %struct2, ptr %y, i32 0, i32 0, i32 1
  store i32 0, ptr %gepn2, align 4
  %gep1 = getelementptr %struct2, ptr %y, i32 0, i32 0, i32 0
  %ret = load i32, ptr %gep1
  ret i32 %ret
}

declare void @bar(ptr sret(%struct1))
