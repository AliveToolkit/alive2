; ModuleID = 'foo.c'
source_filename = "foo.c"
target datalayout = "e-m:o-i64:64-i128:128-n32:64-S128"
target triple = "arm64-apple-macosx13.0.0"

%struct.s = type { i32, i32, i8, i16, i8, i8 }

; Function Attrs: mustprogress nofree norecurse nosync nounwind ssp willreturn memory(argmem: readwrite) uwtable(sync)
define void @f(ptr nocapture noundef %p) local_unnamed_addr #0 {
entry:
  %f1 = getelementptr inbounds %struct.s, ptr %p, i64 0, i32 1
  %0 = load i32, ptr %f1, align 4
  %conv = trunc i32 %0 to i8
  %f5 = getelementptr inbounds %struct.s, ptr %p, i64 0, i32 5
  store i8 %conv, ptr %f5, align 1
  %f2 = getelementptr inbounds %struct.s, ptr %p, i64 0, i32 2
  store i8 %conv, ptr %f2, align 4
  store i32 %0, ptr %p, align 4
  %conv3 = and i32 %0, 255
  %add4 = add nsw i32 %conv3, %0
  %sext = shl i32 %0, 24
  %conv6 = ashr exact i32 %sext, 24
  %add7 = add nsw i32 %add4, %conv6
  %f4 = getelementptr inbounds %struct.s, ptr %p, i64 0, i32 4
  %1 = load i8, ptr %f4, align 4
  %conv8 = zext i8 %1 to i32
  %reass.add = shl nuw nsw i32 %conv8, 1
  %add12 = add i32 %add7, %reass.add
  %conv13 = trunc i32 %add12 to i16
  %f3 = getelementptr inbounds %struct.s, ptr %p, i64 0, i32 3
  store i16 %conv13, ptr %f3, align 2
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
