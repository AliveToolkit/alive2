define iX @src(iX %x) {
    %r = udiv iX %x, 4
    ret iX %r
}

define iX @tgt(iX %x) {
    %r = lshr iX %x, 2
    ret iX %r
}
