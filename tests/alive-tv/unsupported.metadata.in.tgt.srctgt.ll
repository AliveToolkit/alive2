define i1 @src(ptr %a, ptr %b) {
  %l = load i8, ptr %a
  store i8 %l, ptr %b
  ret i1 false
}

define i1 @tgt(ptr %a, ptr %b) {
  %l = load i8, ptr %a, !alias.scope !0, !noalias !3
  store i8 %l, ptr %b, !alias.scope !0, !noalias !3
  ret i1 true
}

; ERROR: Value mismatch

; SKIP-IDENTITY

!0 = !{!1}
!1 = distinct !{!1, !2}
!2 = distinct !{!2, !"LVerDomain"}
!3 = !{!4}
!4 = distinct !{!4, !2}
