define iX @src(iX %x) {
    %r = add iX %x, %x
    ret iX %r
}

define iX @tgt(iX %x) {
    %r = shl iX %x, 1
    ret iX %r
}
