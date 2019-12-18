; TEST-ARGS: -smt-to=2000
target datalayout = "e-i32:32-i128:32-i8:8"

%0 = type [10 x { i32, i128, [5 x { i32, i8, i8 }] }]
; C representation:
; struct S1 { X(i32); Y(i8); Z(i8); }
; struct S2 {A(i32); B(i128); S1[5]; }
; %0 = S2[10]
define i64 @0(%0*) {
; %2 points to: S2[2]->S1[3]->Z
  %2 = getelementptr inbounds %0, %0* %0, i32 0, i32 2, i32 2, i32 3, i32 2
  %3 = ptrtoint %0* %0 to i64
  %4 = ptrtoint i8* %2 to i64
  %5 = sub i64 %4, %3
  ret i64 %5
}
