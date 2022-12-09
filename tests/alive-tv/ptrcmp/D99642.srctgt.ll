define i1 @src() local_unnamed_addr {
entry:
  %a = alloca i64, align 8
  store i64 0, ptr %a, align 8
  %i = ptrtoint ptr %a to i64
  %0 = sub i64 0, %i
  %q = getelementptr i8, ptr %a, i64 %0
  %c = icmp eq ptr %q, null
  ; `%a` is non-null at the end of the block, because we store through it.
  ; However, `%q` is derived from `%a` via a GEP that is not `inbounds`, therefore we cannot judge `%q` is non-null as well
  ; and must retain the `icmp` instruction.
  ret i1 %c 
}

define i1 @tgt() local_unnamed_addr {
if.end:
  ret i1 0
}

; ERROR: Value mismatch
