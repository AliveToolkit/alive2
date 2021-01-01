; TEST-ARGS: -src-unroll=3 -tgt-unroll=3
; https://reviews.llvm.org/D93882
; exact-not-taken cannot be umin(0, m) because m never participates in the exit branch condition.
; Instead, ExactNotTaken should be just 0.

define void @src(i32 %m) {
entry:
    br label %loop
loop:
    %i = phi i32 [0, %entry], [%i.next, %loop]
    %i.next = add i32 %i, 1
    %cond_i = icmp ult i32 %i, 0
    %cond_i2 = icmp ult i32 %i, %m
    %cond = select i1 %cond_i, i1 %cond_i2, i1 false
    br i1 %cond, label %loop, label %exit
exit:
    ret void
}

define void @tgt(i32 %m) {
entry:
    br label %loop
loop:
    %i = phi i32 [0, %entry], [%i.next, %loop]
    %i.next = add i32 %i, 1
    %cond_i = icmp ult i32 %i, 0
    %cond_i2 = icmp ult i32 %i, %m
    %cond = select i1 %cond_i, i1 %cond_i2, i1 false
    br i1 %cond, label %loop, label %exit
exit:
    %n_m_min = call i32 @llvm.umin.i32(i32 0, i32 %m)
    %ExactNotTaken = icmp eq i32 %i, %n_m_min
    call void @llvm.assume(i1 %ExactNotTaken)
    ret void
}

; ERROR: Source is more defined than target

declare void @llvm.assume(i1)
declare i32 @llvm.umin.i32(i32, i32)
