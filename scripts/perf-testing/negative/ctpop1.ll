define iX @src(iX %x) {
    %notx = xor iX -2, %x
    %p1 = call iX @llvm.ctpop.iX (iX %x)
    %p2 = call iX @llvm.ctpop.iX (iX %notx)
    %r = add iX %p1, %p2
    ret iX %r
}

define iX @tgt(iX %x) {
    ret iX X
}

declare iX @llvm.ctpop.iX (iX)
