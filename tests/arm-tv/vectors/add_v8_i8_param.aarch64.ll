define <8 x i8> @vadd(<8 x i8> %a, <8 x i8> %b) {
    %r = add <8 x i8> %a, %b
    ret <8 x i8> %r
}
