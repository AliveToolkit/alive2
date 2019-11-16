@g = constant i16 0

define i8 @f(i1 %cond) {
  br i1 %cond, label %A, label %B
A:
  br label %EXIT
B:
  br label %EXIT
EXIT:
; %__constexpr_0 = bitcast * @g to *
; %__constexpr_1 = bitcast * @g to *
; %__constexpr_2 = gep inbounds * %__constexpr_1, 1 x i64 1
; %addr = phi * [ %__constexpr_0, %A ], [ %__constexpr_2, %B ]
  %addr = phi i8* [getelementptr inbounds (i8, i8* bitcast (i16* @g to i8*), i64 0), %A],
                  [getelementptr inbounds (i8, i8* bitcast (i16* @g to i8*), i64 1), %B]
  %x  = load i8, i8* %addr
  ret i8 %x
}
