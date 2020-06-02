define i1 @src() {
	%guard = freeze i1 undef
	br i1 %guard, label %COMPARE, label %EXIT

COMPARE:
	call void @g()
	ret i1 1

EXIT:
	ret i1 0
}

define i1 @tgt() {
	%guard = freeze i1 undef
	br i1 %guard, label %COMPARE, label %EXIT

COMPARE:
	ret i1 1

EXIT:
	ret i1 0
}

declare void @g()

; ERROR: Mismatch in memory
