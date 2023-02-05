; ModuleID = 'foo.c'
source_filename = "foo.c"
target datalayout = "e-m:o-i64:64-i128:128-n32:64-S128"
target triple = "arm64-apple-macosx13.0.0"

%struct.s = type { i16, i8, i32, i64, i64, i32 }

; Function Attrs: mustprogress nofree norecurse nosync nounwind ssp willreturn memory(argmem: readwrite) uwtable(sync)
define void @f(ptr nocapture noundef %p) local_unnamed_addr #0 {
entry:
  %f5 = getelementptr inbounds %struct.s, ptr %p, i64 0, i32 5
  store i32 0, ptr %f5, align 8
  %f4 = getelementptr inbounds %struct.s, ptr %p, i64 0, i32 4
  %0 = load i64, ptr %f4, align 8
  %sext = shl i64 %0, 32
  %conv6 = ashr exact i64 %sext, 32
  %f3 = getelementptr inbounds %struct.s, ptr %p, i64 0, i32 3
  store i64 %conv6, ptr %f3, align 8
  %sext37 = shl i64 %0, 33
  %conv11 = ashr exact i64 %sext37, 32
  store i64 %conv11, ptr %f4, align 8
  %add15 = add i64 %conv11, %0
  %conv16 = trunc i64 %add15 to i32
  %f2 = getelementptr inbounds %struct.s, ptr %p, i64 0, i32 2
  %1 = load i32, ptr %f2, align 4
  %add17 = add i32 %1, %conv16
  %conv18 = trunc i32 %add17 to i16
  store i16 %conv18, ptr %p, align 8
  ret void
}

attributes #0 = { mustprogress nofree norecurse nosync nounwind ssp willreturn memory(argmem: readwrite) uwtable(sync) "frame-pointer"="non-leaf" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="apple-m1" "target-features"="+aes,+crc,+dotprod,+fp-armv8,+fp16fml,+fullfp16,+lse,+neon,+ras,+rcpc,+rdm,+sha2,+sha3,+v8.1a,+v8.2a,+v8.3a,+v8.4a,+v8.5a,+v8a,+zcm,+zcz" }

!llvm.module.flags = !{!0, !1, !2, !3, !4}
!llvm.ident = !{!5}

!0 = !{i32 2, !"SDK Version", [2 x i32] [i32 13, i32 1]}
!1 = !{i32 1, !"wchar_size", i32 4}
!2 = !{i32 8, !"PIC Level", i32 2}
!3 = !{i32 7, !"uwtable", i32 1}
!4 = !{i32 7, !"frame-pointer", i32 1}
!5 = !{!"clang version 17.0.0 (git@github.com:llvm/llvm-project.git c32022ad260aa1e6f485c5a6820fa9973f3b108e)"}
