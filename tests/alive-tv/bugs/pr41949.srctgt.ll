; https://bugs.llvm.org/show_bug.cgi?id=41949 with minor fixes
;source_filename = "41949.ll"
target datalayout = "E"

define i32 @src(i32* %p) {
  %u = alloca i32
  %a = bitcast i32* %u to i32*
  %b = bitcast i32* %u to i12*
  store i32 -1, i32* %a
  store i12 20, i12* %b
  %v = load i32, i32* %u
  ret i32 %v
}

define i32 @tgt(i32* %p) {
  %u = alloca i32
  %a = bitcast i32* %u to i32*
  store i32 22020095, i32* %a
  %v = load i32, i32* %u
  ret i32 %v
} 
; ERROR: Value mismatch
