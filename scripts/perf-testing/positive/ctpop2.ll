define i1 @src(iX %x) {
    %p1 = call iX @llvm.ctpop.iX (iX %x)
    %r = icmp sle iX %p1, X
    ret i1 %r
}

define i1 @tgt(iX %x) {
    ret i1 true
}

declare iX @llvm.ctpop.iX (iX)
