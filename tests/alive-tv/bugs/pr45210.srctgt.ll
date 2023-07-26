; Found by Alive2
target datalayout="p:32:32:32-p1:16:16:16-n8:16:32:64"

@G16 = internal constant [10 x i16] [i16 35, i16 82, i16 69, i16 81, i16 85,
                                     i16 73, i16 82, i16 69, i16 68, i16 0]

define i1 @src(i32 %X) {
  %P = getelementptr [10 x i16], ptr @G16, i32 0, i32 %X
  %Q = load i16, ptr %P
  %R = icmp eq i16 %Q, 0
  ret i1 %R
}

define i1 @tgt(i32 %X) {
  %R = icmp eq i32 %X, 9
  ret i1 %R
}

; ERROR: Value mismatch
