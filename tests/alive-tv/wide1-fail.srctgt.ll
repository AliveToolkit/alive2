; ERROR: Value mismatch

define i128 @src(i128 %x) {
    %r = ashr i128 %x, 5
    ret i128 %r
}

define i128 @tgt(i128 %x) {
    %r = udiv i128 %x, 32
    ret i128 %r
}
