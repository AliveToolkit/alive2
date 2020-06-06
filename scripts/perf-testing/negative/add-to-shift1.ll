define iX @src(iX %x) {
    %r = add iX %x, %x
    ret iX %r
}

define iX @tgt(iX %x) {
    %r = shl iX %x, 3
    ret iX %r
}
