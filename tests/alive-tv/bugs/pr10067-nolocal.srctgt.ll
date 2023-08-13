%struct1 = type { i32, i32 }
%struct2 = type { %struct1, ptr }

define i32 @src(%struct1 %init_val) {
  %x = alloca %struct1, align 8
  %y = alloca %struct2, align 8
  store %struct1 %init_val, ptr %x

  %gepn1 = getelementptr inbounds %struct2, ptr %y, i32 0, i32 0, i32 0
  store i32 0, ptr %gepn1, align 8
  %gepn2 = getelementptr inbounds %struct2, ptr %y, i32 0, i32 0, i32 1
  store i32 0, ptr %gepn2, align 4

  %bit1 = bitcast ptr %x to ptr
  %bit2 = bitcast ptr %y to ptr
  %load = load i64, ptr %bit1, align 8
  store i64 %load, ptr %bit2, align 8

  %gep1 = getelementptr %struct2, ptr %y, i32 0, i32 0, i32 0
  %ret = load i32, ptr %gep1
  ret i32 %ret
}

define i32 @tgt(%struct1 %init_val) {
  %x = alloca %struct1, align 8
  %y = alloca %struct2, align 8
  %y1 = bitcast ptr %y to ptr
  store %struct1 %init_val, ptr %y1
  %gepn1 = getelementptr inbounds %struct2, ptr %y, i32 0, i32 0, i32 0
  store i32 0, ptr %gepn1, align 8
  %gepn2 = getelementptr inbounds %struct2, ptr %y, i32 0, i32 0, i32 1
  store i32 0, ptr %gepn2, align 4
  %bit1 = bitcast ptr %x to ptr
  %bit2 = bitcast ptr %y to ptr
  %gep1 = getelementptr %struct2, ptr %y, i32 0, i32 0, i32 0
  %ret = load i32, ptr %gep1
  ret i32 %ret
}

; ERROR: Value mismatch
