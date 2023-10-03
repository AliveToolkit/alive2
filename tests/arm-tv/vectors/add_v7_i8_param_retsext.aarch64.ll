define signext <7 x i8> @vadd(<7 x i8> %a, <7 x i8> %b) {
    %r = add <7 x i8> %a, %b
    ret <7 x i8> %r
}
