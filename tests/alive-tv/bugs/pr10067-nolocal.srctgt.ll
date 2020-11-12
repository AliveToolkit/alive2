target datalayout = "e-p:32:32:32-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:32:64-f32:32:32-f64:32:64-v64:64:64-v128:128:128-a0:0:64-f80:128:128-n8:16:32"
target triple = "i386-apple-darwin10"

%struct1 = type { i32, i32 }
%struct2 = type { %struct1, i8* }

define i32 @src(%struct1 %init_val) noinline {
  %x = alloca %struct1, align 8
  %y = alloca %struct2, align 8
  store %struct1 %init_val, %struct1* %x

  %gepn1 = getelementptr inbounds %struct2, %struct2* %y, i32 0, i32 0, i32 0
  store i32 0, i32* %gepn1, align 8
  %gepn2 = getelementptr inbounds %struct2, %struct2* %y, i32 0, i32 0, i32 1
  store i32 0, i32* %gepn2, align 4

  %bit1 = bitcast %struct1* %x to i64*
  %bit2 = bitcast %struct2* %y to i64*
  %load = load i64, i64* %bit1, align 8
  store i64 %load, i64* %bit2, align 8

  %gep1 = getelementptr %struct2, %struct2* %y, i32 0, i32 0, i32 0
  %ret = load i32, i32* %gep1
  ret i32 %ret
}

define i32 @tgt(%struct1 %init_val) noinline {
  %x = alloca %struct1, align 8
  %y = alloca %struct2, align 8
  %y1 = bitcast %struct2* %y to %struct1*
  store %struct1 %init_val, %struct1* %y1
  %gepn1 = getelementptr inbounds %struct2, %struct2* %y, i32 0, i32 0, i32 0
  store i32 0, i32* %gepn1, align 8
  %gepn2 = getelementptr inbounds %struct2, %struct2* %y, i32 0, i32 0, i32 1
  store i32 0, i32* %gepn2, align 4
  %bit1 = bitcast %struct1* %x to i64*
  %bit2 = bitcast %struct2* %y to i64*
  %gep1 = getelementptr %struct2, %struct2* %y, i32 0, i32 0, i32 0
  %ret = load i32, i32* %gep1
  ret i32 %ret
}

; ERROR: Value mismatch
