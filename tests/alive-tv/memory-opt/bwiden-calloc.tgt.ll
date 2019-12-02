define i32 @calloc_init(i32 %x) {
  %ptr0 = call noalias i8* @calloc(i64 1, i64 4)
  %ptr = bitcast i8* %ptr0 to i32*
  store i32 %x, i32* %ptr, align 4
  call void @free(i8* %ptr0)
  ret i32 %x
}

; calloc's bwiden is not supported yet
; CHECK: bits_byte: 8

declare noalias i8* @calloc(i64, i64)
declare void @free(i8*)
