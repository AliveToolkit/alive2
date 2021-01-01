; TEST-ARGS: -src-unroll=3 -tgt-unroll=3
; https://reviews.llvm.org/D93882
; exact-not-taken is umax(n, m) because both conditions (cond_i, cond_i2) participate in branching,
; preventing them from being poison.

define void @src(i32 %n, i32 %m) {
entry:
    br label %loop
loop:
    %i = phi i32 [0, %entry], [%i.next, %loop]
    %i.next = add i32 %i, 1
    %cond_i = icmp ult i32 %i, %n
    %cond_i2 = icmp ult i32 %i, %m
    %cond_or = select i1 %cond_i, i1 true, i1 %cond_i2
    br i1 %cond_or, label %loop, label %exit
exit:
    ret void
}

define void @tgt(i32 %n, i32 %m) {
entry:
    br label %loop
loop:
    %i = phi i32 [0, %entry], [%i.next, %loop]
    %i.next = add i32 %i, 1
    %cond_i = icmp ult i32 %i, %n
    %cond_i2 = icmp ult i32 %i, %m
    %cond_or = select i1 %cond_i, i1 true, i1 %cond_i2
    br i1 %cond_or, label %loop, label %exit
exit:
    %n_m_max = call i32 @llvm.umax.i32(i32 %n, i32 %m)
    %ExactNotTaken = icmp eq i32 %i, %n_m_max
    call void @llvm.assume(i1 %ExactNotTaken)
    ret void
}

declare void @llvm.assume(i1)
declare i32 @llvm.umax.i32(i32, i32)
