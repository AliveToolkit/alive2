; target: 64 bits ptr addr
target datalayout = "e-p:64:64"

define i1 @malloc_oom() {
  %ptr1 = call noalias i8* @malloc(i64 9223372036854775552) ; 7FFF FFFF FFFF FF00
  %ptr2 = call noalias i8* @malloc(i64 9223372036854775552) ; 7FFF FFFF FFFF FF00
  ret i1 true
}

declare noalias i8* @malloc(i64)
