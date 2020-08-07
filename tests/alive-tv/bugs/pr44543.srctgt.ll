; Found by Alive2
; SKIP-IDENTITY

target datalayout = "p:64:64:64-i64:32:32"

define i64* @src(i8* %x) {
entry:
  %p = bitcast i8* %x to i64*
  %b1 = load i64, i64* %p
  %p2 = inttoptr i64 %b1 to i64*
  ret i64* %p2
}

define i64* @tgt(i8* %x) {
entry:
  %0 = bitcast i8* %x to i64**
  %b11 = load i64*, i64** %0, align 8
  ret i64* %b11
}

; ERROR: Source is more defined than target
