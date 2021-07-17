; fptoui does rounding towards zero conversion when it ties

define i32 @src() {
	%v = fptoui float 31.5 to i32
	ret i32 %v
}

define i32 @tgt() {
	ret i32 31
}
