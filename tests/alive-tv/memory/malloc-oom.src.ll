; target: 64 bits ptr addr
target datalayout = "e-p:64:64"

define i1 @malloc_oom() {
  %ptr1 = call noalias i8* @malloc(i64 9223372036854775552) ; 7FFF FFFF FFFF FF00
  %ptr2 = call noalias i8* @malloc(i64 9223372036854775552) ; 7FFF FFFF FFFF FF00
  %i1 = ptrtoint i8* %ptr1 to i64
  %i2 = ptrtoint i8* %ptr2 to i64
  %eq1 = icmp ne i64 %i1, 0
  %eq2 = icmp ne i64 %i2, 0
  %res = and i1 %eq1, %eq2
  ret i1 %res
}

; ERROR: Value mismatch

declare noalias i8* @malloc(i64)
