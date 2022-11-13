; Found by Alive2

target datalayout = "e-i8:8:8-i16:16:16"
target triple = "x86_64-unknown-unknown"
declare i32 @memcmp(ptr nocapture, ptr nocapture, i64)

define i32 @src(ptr nocapture readonly %x, ptr nocapture readonly %y)  {
  %call = tail call i32 @memcmp(ptr %x, ptr %y, i64 2)
  ret i32 %call
}

define i32 @tgt(ptr nocapture readonly %x, ptr nocapture readonly %y) {
  %1 = bitcast ptr %x to i16*
  %2 = bitcast ptr %y to i16*
  %3 = load i16, i16* %1
  %4 = load i16, i16* %2
  %5 = call i16 @llvm.bswap.i16(i16 %3)
  %6 = call i16 @llvm.bswap.i16(i16 %4)
  %7 = zext i16 %5 to i32
  %8 = zext i16 %6 to i32
  %9 = sub i32 %7, %8
  ret i32 %9
}

declare i16 @llvm.bswap.i16(i16) nounwind memory(none) willreturn

; ERROR: Source is more defined than target
