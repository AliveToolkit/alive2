; TEST-ARGS: -src-unroll=3 -tgt-unroll=3
; https://reviews.llvm.org/D93882
; exact-not-taken cannot be umin(n, m) because it is possible for (n, m) to be (0, poison)

define void @src(i32 %n, i32 %m) {
entry:
    br label %loop
loop:
    %i = phi i32 [0, %entry], [%i.next, %loop]
    %i.next = add i32 %i, 1
    %cond_i = icmp uge i32 %i, %n
    %cond_i2 = icmp uge i32 %i, %m
    %cond_or = select i1 %cond_i, i1 true, i1 %cond_i2
    br i1 %cond_or, label %exit, label %loop
exit:
    ret void
}

define void @tgt(i32 %n, i32 %m) {
entry:
    br label %loop
loop:
    %i = phi i32 [0, %entry], [%i.next, %loop]
    %i.next = add i32 %i, 1
    %cond_i = icmp uge i32 %i, %n
    %cond_i2 = icmp uge i32 %i, %m
    %cond_or = select i1 %cond_i, i1 true, i1 %cond_i2
    br i1 %cond_or, label %exit, label %loop
exit:
    %n_m_min = call i32 @llvm.umin.i32(i32 %n, i32 %m)
    %ExactNotTaken = icmp eq i32 %i, %n_m_min
    call void @llvm.assume(i1 %ExactNotTaken)
    ret void
}

; ERROR: Source is more defined than target

declare void @llvm.assume(i1)
declare i32 @llvm.umin.i32(i32, i32)
