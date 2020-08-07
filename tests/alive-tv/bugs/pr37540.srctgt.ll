; https://bugs.llvm.org/show_bug.cgi?id=37540
; To check this, !nonnull support is needed

define i1 @src({}** %arg, i1 %arg2) {
    br i1 %arg2, label %bb1, label %bb2

bb1:
    %load1 = load {}*, {}** %arg
    %cmp1 = icmp eq {}* %load1, null
    ret i1 %cmp1

bb2:
    %load2 = load {}*, {}** %arg, !nonnull !0
    %cmp2 = icmp eq {}* %load2, null
    ret i1 %cmp2
}

!0 = !{}

define i1 @tgt({}** %arg, i1 %arg2) {
  br i1 %arg2, label %bb1, label %bb2

bb1:                                              ; preds = %0
  ret i1 false

bb2:                                              ; preds = %0
  ret i1 false
}

; XFAIL: Unsupported metadata: 11
; SKIP-IDENTITY
