target datalayout = "e-i32:32-i128:32-i8:8"

%0 = type [10 x { i32, i128, [5 x { i32, i8, i8 }] }]
; C representation:
; struct S1 { X(i32); Y(i8); Z(i8); }
; struct S2 {A(i32); B(i128); S1[5]; }
; %0 = S2[10]
define i64 @0(%0* nocapture) {
; %2 points to: S2[2]->S1[3]->Z
  ret i64 169
}
