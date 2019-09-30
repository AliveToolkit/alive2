; target: 64 bits ptr addr
target datalayout = "e-m:o-i64:64-f80:128-n8:16:32:64-S128"

define i64 @malloc_null() {
  %ptr1 = call noalias i8* @malloc(i64 1)
  %ptr2 = call noalias i8* @malloc(i64 1)
  %ptr3 = call noalias i8* @malloc(i64 1)
  %ptr4 = call noalias i8* @malloc(i64 1)
  %ptr5 = call noalias i8* @malloc(i64 1)
  %ptr6 = call noalias i8* @malloc(i64 1)
  %ptr7 = call noalias i8* @malloc(i64 1)
  %i1 = ptrtoint i8* %ptr1 to i64
  %i2 = ptrtoint i8* %ptr2 to i64
  %i3 = ptrtoint i8* %ptr3 to i64
  %i4 = ptrtoint i8* %ptr4 to i64
  %i5 = ptrtoint i8* %ptr5 to i64
  %i6 = ptrtoint i8* %ptr6 to i64
  %i7 = ptrtoint i8* %ptr7 to i64
  %cmp1 = icmp eq i64 %i1, 0
  %cmp2 = icmp eq i64 %i2, 0
  %cmp3 = icmp eq i64 %i3, 0
  %cmp4 = icmp eq i64 %i4, 0
  %cmp5 = icmp eq i64 %i5, 0
  %cmp6 = icmp eq i64 %i6, 0
  %cmp7 = icmp eq i64 %i7, 0

  br i1 %cmp1, label %A1, label %A2
A1:
  br label %A3
A2:
  br label %A3
A3:
  %x1 = phi i64 [1, %A1], [2, %A2]

  br i1 %cmp2, label %B1, label %B2
B1:
  br label %B3
B2:
  br label %B3
B3:
  %x2 = phi i64 [3, %B1], [4, %B2]

  br i1 %cmp3, label %C1, label %C2
C1:
  br label %C3
C2:
  br label %C3
C3:
  %x3 = phi i64 [5, %C1], [6, %C2]

  br i1 %cmp4, label %D1, label %D2
D1:
  br label %D3
D2:
  br label %D3
D3:
  %x4 = phi i64 [7, %D1], [8, %D2]
 
  br i1 %cmp5, label %E1, label %E2
E1:
  br label %E3
E2:
  br label %E3
E3:
  %x5 = phi i64 [9, %E1], [10, %E2]
 
  br i1 %cmp6, label %F1, label %F2
F1:
  br label %F3
F2:
  br label %F3
F3:
  %x6 = phi i64 [11, %F1], [12, %F2]

  br i1 %cmp7, label %G1, label %G2
G1:
  br label %G3
G2:
  br label %G3
G3:
  %x7 = phi i64 [12, %G1], [14, %G2]

  %sum.0 = add i64 %x1, %x2
  %sum.1 = add i64 %sum.0, %x3
  %sum.2 = add i64 %sum.1, %x4
  %sum.3 = add i64 %sum.2, %x5
  %sum.4 = add i64 %sum.3, %x6
  %sum.5 = add i64 %sum.4, %x7
  ret i64 %sum.5
}

declare noalias i8* @malloc(i64)
