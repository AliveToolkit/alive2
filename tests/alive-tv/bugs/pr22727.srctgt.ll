; This test shows that pr22727 was actually not a bug. :)

target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

define i32 @src(i32* %bp) {
entry:
  %bp.addr = alloca i32*, align 8
  %lbp = alloca i32*, align 8
  %lbp1 = alloca double, align 8
  store i32* %bp, i32** %bp.addr, align 8
  %0 = load i32*, i32** %bp.addr, align 8
  %add.ptr = getelementptr inbounds i32, i32* %0, i64 1
  store i32* %add.ptr, i32** %lbp, align 8
  %1 = load i32*, i32** %lbp, align 8
  %2 = ptrtoint i32* %1 to i64
  %conv = uitofp i64 %2 to double
  store double %conv, double* %lbp1, align 8
  %3 = load double, double* %lbp1, align 8
  %cmp = fcmp ogt double %3, 0.000000e+00
  br i1 %cmp, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  %4 = load i32*, i32** %lbp, align 8
  %isnull = icmp eq i32* %4, null
  br i1 %isnull, label %delete.end, label %delete.notnull

delete.notnull:                                   ; preds = %if.then
  %5 = bitcast i32* %4 to i8*
  call void @_ZdaPv(i8* %5)
  br label %delete.end

delete.end:                                       ; preds = %delete.notnull, %if.then
  br label %if.end

if.end:                                           ; preds = %delete.end, %entry
  ret i32 0
}

define i32 @tgt(i32* %bp) {
entry:
  %add.ptr = getelementptr inbounds i32, i32* %bp, i64 1
  %0 = bitcast i32* %add.ptr to i8*
  tail call void @_ZdaPv(i8* %0)
  ret i32 0
}

declare void @_ZdaPv(i8*)
