; https://bugs.llvm.org/show_bug.cgi?id=4374
; TEST-ARGS: -disable-undef-input

define float @src(float %a, float %b) nounwind {
entry:
        %tmp3 = fsub float %a, %b                ; <float> [#uses=1]
        %tmp4 = fsub float -0.000000e+00, %tmp3          ; <float> [#uses=1]
        ret float %tmp4
}

define float @tgt(float %a, float %b) nounwind {
entry:
        %tmp3 = fsub float %b, %a                ; <float> [#uses=1]
        ret float %tmp3
}

; ERROR: Value mismatch
