; TEST-ARGS: -src-unroll=3 -tgt-unroll=3
; https://reviews.llvm.org/D93882
; If m is constant, exact-not-taken is umin(n, m)

define void @src(i32 %n) {
entry:
    br label %loop
loop:
    %i = phi i32 [0, %entry], [%i.next, %loop]
    %i.next = add i32 %i, 1
    %cond_i = icmp ult i32 %i, %n
    %cond_i2 = icmp ult i32 %i, 2
    %cond = select i1 %cond_i, i1 %cond_i2, i1 false
    br i1 %cond, label %loop, label %exit
exit:
    ret void
}

define void @tgt(i32 %n) {
entry:
    br label %loop
loop:
    %i = phi i32 [0, %entry], [%i.next, %loop]
    %i.next = add i32 %i, 1
    %cond_i = icmp ult i32 %i, %n
    %cond_i2 = icmp ult i32 %i, 2
    %cond = select i1 %cond_i, i1 %cond_i2, i1 false
    br i1 %cond, label %loop, label %exit
exit:
    %n_m_min = call i32 @llvm.umin.i32(i32 %n, i32 2)
    %ExactNotTaken = icmp eq i32 %i, %n_m_min
    call void @llvm.assume(i1 %ExactNotTaken)
    ret void
}

declare void @llvm.assume(i1)
declare i32 @llvm.umin.i32(i32, i32)
