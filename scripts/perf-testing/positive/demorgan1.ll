define iX @src(iX %p, iX %q) {
    %notp = xor iX %p, -1
    %notq = xor iX %q, -1
    %r = and iX %notp, %notq
    ret iX %r
}

define iX @tgt(iX %p, iX %q) {
    %r = or iX %p, %q
    %notr = xor iX %r, -1
    ret iX %notr
}
