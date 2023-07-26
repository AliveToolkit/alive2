; Reported by Alive2

define i64 @src(ptr %foo, i64 %i, i64 %j) {
  %gep1 = getelementptr inbounds i32, ptr %foo, i64 %i
  %gep2 = getelementptr inbounds i8, ptr %foo, i64 %j
  %cast1 = ptrtoint ptr %gep1 to i64
  %cast2 = ptrtoint ptr %gep2 to i64
  %sub = sub i64 %cast1, %cast2
  ret i64 %sub
}

define i64 @tgt(ptr %foo, i64 %i, i64 %j) {
  %gep1.idx = shl nuw i64 %i, 2
  %x = sub i64 %gep1.idx, %j
  ret i64 %x
}

; ERROR: Target is more poisonous than source
