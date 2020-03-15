; https://bugs.llvm.org/show_bug.cgi?id=1261
; This shows that pr1261 was not a bug. :) (which matches with the final conclusion)

define i16 @src(i31 %zzz) {
entry:
	%A = sext i31 %zzz to i32
	%B = add i32 %A, 16384
	%C = lshr i32 %B, 15
	%D = trunc i32 %C to i16
	ret i16 %D
}

define i16 @tgt(i31 %zzz) {
entry:
        %A1 = zext i31 %zzz to i32              ; <i32> [#uses=1]
        %B = add i32 %A1, 16384         ; <i32> [#uses=1]
        %C = lshr i32 %B, 15            ; <i32> [#uses=1]
        %D = trunc i32 %C to i16                ; <i16> [#uses=1]
        ret i16 %D
}
