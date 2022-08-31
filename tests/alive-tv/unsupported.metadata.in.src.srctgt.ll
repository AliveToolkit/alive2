define i1 @src(i8* %a, i8* %b) {
  %l = load i8, i8* %a, !alias.scope !0, !noalias !3
  store i8 %l, i8* %b, !alias.scope !0, !noalias !3
  ret i1 false
}

define i1 @tgt(i8* %a, i8* %b) {
  %l = load i8, i8* %a, !alias.scope !0, !noalias !3
  store i8 %l, i8* %b, !alias.scope !0, !noalias !3
  ret i1 true
}

; ERROR: Unsupported metadata: 7

; SKIP-IDENTITY

!0 = !{!1}
!1 = distinct !{!1, !2}
!2 = distinct !{!2, !"LVerDomain"}
!3 = !{!4}
!4 = distinct !{!4, !2}
