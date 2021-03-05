define i8 @src(i8* %p) {
entry:
    call void @llvm.lifetime.start.p0i8(i64 1, i8* %p)
    store i8 1, i8* %p
    call void @llvm.lifetime.start.p0i8(i64 1, i8* %p)
    %v = load i8, i8* %p
    call void @llvm.lifetime.end.p0i8(i64 1, i8* %p)
    ret i8 %v
}

define i8 @tgt(i8* %p) {
  call void @llvm.lifetime.start.p0i8(i64 1, i8* %p)
  store i8 1, i8* %p, align 1
  call void @llvm.lifetime.start.p0i8(i64 1, i8* %p)
  call void @llvm.lifetime.end.p0i8(i64 1, i8* %p)
  ret i8 undef
}

declare void @llvm.lifetime.start.p0i8(i64, i8*)
declare void @llvm.lifetime.end.p0i8(i64, i8*)

; WIP: deal with the second lifetime.start
; ERROR: Target's return value is more undefined
