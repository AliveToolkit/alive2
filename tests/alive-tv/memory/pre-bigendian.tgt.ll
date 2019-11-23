target datalayout = "E-p:32:32:32-p1:16:16:16-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:32:64-f32:32:32-f64:64:64-n32"

define i8 @coerce_mustalias_pre0(i16* %P, i1 %cond) {
  %P3 = bitcast i16* %P to i8*
  br i1 %cond, label %T, label %F

T:                                                ; preds = %0
  store i16 42, i16* %P
  br label %Cont

F:                                                ; preds = %0
  %A.pre = load i8, i8* %P3
  br label %Cont

Cont:                                             ; preds = %F, %T
  %A = phi i8 [ %A.pre, %F ], [ 0, %T ]
  ret i8 %A
}
