; ModuleID = 'stack1.c'
source_filename = "stack1.c"
target datalayout = "e-m:o-i64:64-i128:128-n32:64-S128"
target triple = "arm64-apple-macosx13.0.0"

; Function Attrs: mustprogress nofree nosync nounwind willreturn memory(none) uwtable(sync)
define i32 @foo(i32 noundef %x) local_unnamed_addr #0 {
entry:
  %a = alloca [10 x i32], align 4
  call void @llvm.lifetime.start.p0(i64 40, ptr nonnull %a) #2
  store i32 3, ptr %a, align 4, !tbaa !6
  %arrayidx1 = getelementptr inbounds [10 x i32], ptr %a, i64 0, i64 1
  store i32 566, ptr %arrayidx1, align 4, !tbaa !6
  %arrayidx2 = getelementptr inbounds [10 x i32], ptr %a, i64 0, i64 2
  store i32 345234, ptr %arrayidx2, align 4, !tbaa !6
  %arrayidx3 = getelementptr inbounds [10 x i32], ptr %a, i64 0, i64 4
  store i32 2, ptr %arrayidx3, align 4, !tbaa !6
  %arrayidx4 = getelementptr inbounds [10 x i32], ptr %a, i64 0, i64 5
  store i32 0, ptr %arrayidx4, align 4, !tbaa !6
  %arrayidx5 = getelementptr inbounds [10 x i32], ptr %a, i64 0, i64 6
  store i32 564, ptr %arrayidx5, align 4, !tbaa !6
  %arrayidx6 = getelementptr inbounds [10 x i32], ptr %a, i64 0, i64 7
  store i32 453632, ptr %arrayidx6, align 4, !tbaa !6
  %arrayidx7 = getelementptr inbounds [10 x i32], ptr %a, i64 0, i64 8
  store i32 345, ptr %arrayidx7, align 4, !tbaa !6
  %arrayidx8 = getelementptr inbounds [10 x i32], ptr %a, i64 0, i64 9
  store i32 345, ptr %arrayidx8, align 4, !tbaa !6
  %idxprom = zext i32 %x to i64
  %arrayidx9 = getelementptr inbounds [10 x i32], ptr %a, i64 0, i64 %idxprom
  %0 = load i32, ptr %arrayidx9, align 4, !tbaa !6
  call void @llvm.lifetime.end.p0(i64 40, ptr nonnull %a) #2
  ret i32 %0
}

; Function Attrs: mustprogress nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
declare void @llvm.lifetime.start.p0(i64 immarg, ptr nocapture) #1

; Function Attrs: mustprogress nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
declare void @llvm.lifetime.end.p0(i64 immarg, ptr nocapture) #1

attributes #0 = { mustprogress nofree nosync nounwind willreturn memory(none) uwtable(sync) "frame-pointer"="non-leaf" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="apple-m1" "target-features"="+aes,+crc,+dotprod,+fp-armv8,+fp16fml,+fullfp16,+lse,+neon,+ras,+rcpc,+rdm,+sha2,+sha3,+v8.1a,+v8.2a,+v8.3a,+v8.4a,+v8.5a,+v8a,+zcm,+zcz" }
attributes #1 = { mustprogress nocallback nofree nosync nounwind willreturn memory(argmem: readwrite) }
attributes #2 = { nounwind }

!llvm.module.flags = !{!0, !1, !2, !3, !4}
!llvm.ident = !{!5}

!0 = !{i32 2, !"SDK Version", [2 x i32] [i32 13, i32 1]}
!1 = !{i32 1, !"wchar_size", i32 4}
!2 = !{i32 8, !"PIC Level", i32 2}
!3 = !{i32 7, !"uwtable", i32 1}
!4 = !{i32 7, !"frame-pointer", i32 1}
!5 = !{!"clang version 17.0.0 (git@github.com:llvm/llvm-project.git 9030e7d5543fb198b1f3977d757dbfd5543ee465)"}
!6 = !{!7, !7, i64 0}
!7 = !{!"int", !8, i64 0}
!8 = !{!"omnipotent char", !9, i64 0}
!9 = !{!"Simple C/C++ TBAA"}
