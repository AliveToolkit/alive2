define i32 @src() {
	%v = fptosi float 31.5 to i32
	ret i32 %v
}

define i32 @tgt() {
	ret i32 poison
}

; ERROR: Target is more poisonous than source
