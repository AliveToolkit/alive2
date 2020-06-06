; ERROR: Value mismatch

define i255 @src(i255 %x) {
    %r = sub i255 0, %x
    ret i255 %r
}

define i255 @tgt(i255 %x) {
    %r = mul i255 %x, -7
    ret i255 %r
}
