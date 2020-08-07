; Reported by Alive2
; TEST-ARGS: -disable-undef-input

define i64 @src(i8* %foo, i64 %i, i64 %j) {
  %bit = bitcast i8* %foo to i32*
  %gep1 = getelementptr inbounds i32, i32* %bit, i64 %i
  %gep2 = getelementptr inbounds i8, i8* %foo, i64 %j
  %cast1 = ptrtoint i32* %gep1 to i64
  %cast2 = ptrtoint i8* %gep2 to i64
  %sub = sub i64 %cast1, %cast2
  ret i64 %sub
}

define i64 @tgt(i8* %foo, i64 %i, i64 %j) {
  %gep1.idx = shl nuw i64 %i, 2
  %x = sub i64 %gep1.idx, %j
  ret i64 %x
}

; ERROR: Target is more poisonous than source
