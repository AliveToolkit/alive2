target datalayout = "E-p:32:32:32-p1:16:16:16-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:32:64-f32:32:32-f64:64:64-n32"

define i8 @coerce_mustalias_pre0(ptr %P, i1 %cond) {
  br i1 %cond, label %T, label %F

T:
  store i16 42, ptr %P
  br label %Cont

F:
  %A.pre = load i8, ptr %P
  br label %Cont

Cont:
  %A = phi i8 [ %A.pre, %F ], [ 0, %T ]
  ret i8 %A
}
