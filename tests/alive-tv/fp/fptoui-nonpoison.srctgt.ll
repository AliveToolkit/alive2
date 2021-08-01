; https://reviews.llvm.org/D106053#2885236

define i32 @src() {
	%v = fptoui float 31.5 to i32
	ret i32 %v
}

define i32 @tgt() {
	ret i32 poison
}

; ERROR: Target is more poisonous than source
