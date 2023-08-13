; This test shows that pr22727 was actually not a bug. :)

target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

define i32 @src(ptr %bp) {
entry:
  %bp.addr = alloca ptr, align 8
  %lbp = alloca ptr, align 8
  %lbp1 = alloca double, align 8
  store ptr %bp, ptr %bp.addr, align 8
  %0 = load ptr, ptr %bp.addr, align 8
  %add.ptr = getelementptr inbounds i32, ptr %0, i64 1
  store ptr %add.ptr, ptr %lbp, align 8
  %1 = load ptr, ptr %lbp, align 8
  %2 = ptrtoint ptr %1 to i64
  %conv = uitofp i64 %2 to double
  store double %conv, ptr %lbp1, align 8
  %3 = load double, ptr %lbp1, align 8
  %cmp = fcmp ogt double %3, 0.000000e+00
  br i1 %cmp, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  %4 = load ptr, ptr %lbp, align 8
  %isnull = icmp eq ptr %4, null
  br i1 %isnull, label %delete.end, label %delete.notnull

delete.notnull:                                   ; preds = %if.then
  call void @_ZdaPv(ptr %4)
  br label %delete.end

delete.end:                                       ; preds = %delete.notnull, %if.then
  br label %if.end

if.end:                                           ; preds = %delete.end, %entry
  ret i32 0
}

define i32 @tgt(ptr %bp) {
entry:
  %add.ptr = getelementptr inbounds i32, ptr %bp, i64 1
  tail call void @_ZdaPv(ptr %add.ptr)
  ret i32 0
}

declare void @_ZdaPv(ptr)
