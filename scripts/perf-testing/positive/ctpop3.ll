define i1 @src(iX %x) {
    %p1 = call iX @llvm.ctpop.iX (iX %x)
    %r = icmp slt iX %p1, X
    ret i1 %r
}

define i1 @tgt(iX %x) {
    %r = icmp ne iX %x, -1
    ret i1 %r
}

declare iX @llvm.ctpop.iX (iX)
