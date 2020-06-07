define iX @src(iX %x) {
    %r1 = add iX %x, %x
    %r2 = add iX %r1, %r1
    %r3 = add iX %r2, %r2
    ret iX %r3
}

define iX @tgt(iX %x) {
    %r = shl iX %x, 3
    ret iX %r
}
