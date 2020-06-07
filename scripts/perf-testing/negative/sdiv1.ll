define iX @src(iX %x) {
    %r = sdiv iX %x, 4
    ret iX %r
}

define iX @tgt(iX %x) {
    %ispos = icmp sgt iX %x, 0
    %pos = ashr iX %x, 2
    %offset = add iX %x, 2
    %neg = ashr iX %offset, 2
    %r = select i1 %ispos, iX %pos, iX %neg
    ret iX %r
}
