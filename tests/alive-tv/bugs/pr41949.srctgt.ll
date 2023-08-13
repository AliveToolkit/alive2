; https://bugs.llvm.org/show_bug.cgi?id=41949 with minor fixes
target datalayout = "E"

define i32 @src(ptr %p) {
  %u = alloca i32
  %a = bitcast ptr %u to ptr
  %b = bitcast ptr %u to ptr
  store i32 -1, ptr %a
  store i12 20, ptr %b
  %v = load i32, ptr %u
  ret i32 %v
}

define i32 @tgt(ptr %p) {
  %u = alloca i32
  %a = bitcast ptr %u to ptr
  store i32 22020095, ptr %a
  %v = load i32, ptr %u
  ret i32 %v
} 
; ERROR: Value mismatch
