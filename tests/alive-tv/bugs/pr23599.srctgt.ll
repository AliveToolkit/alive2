; https://bugs.llvm.org/show_bug.cgi?id=23599
; To reproduce this, bytes of escaped local blocks should be checked

target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%struct.ether_addr = type { [6 x i8] }
%struct.ether_header = type { [6 x i8], [6 x i8] }

; Function Attrs: uwtable
define void @src(%struct.ether_addr* nocapture readonly %ether_src, %struct.ether_addr* nocapture readonly %ether_dst) #0 {
entry:
  %eth = alloca %struct.ether_header, align 1
  %0 = getelementptr inbounds %struct.ether_header, %struct.ether_header* %eth, i64 0, i32 0, i64 0
  call void @llvm.lifetime.start(i64 12, i8* %0) #1
  call void @llvm.memset.p0i8.i64(i8* %0, i8 0, i64 12, i32 1, i1 false)
  %arraydecay = getelementptr inbounds %struct.ether_header, %struct.ether_header* %eth, i64 0, i32 1, i64 0
  %1 = getelementptr inbounds %struct.ether_addr, %struct.ether_addr* %ether_src, i64 0, i32 0, i64 0
  call void @llvm.memcpy.p0i8.p0i8.i64(i8* %arraydecay, i8* %1, i64 6, i32 1, i1 false), !tbaa.struct !1
  %2 = getelementptr inbounds %struct.ether_addr, %struct.ether_addr* %ether_dst, i64 0, i32 0, i64 0
  call void @llvm.memcpy.p0i8.p0i8.i64(i8* %0, i8* %2, i64 6, i32 1, i1 false), !tbaa.struct !1
  call void @_Z5PrintRK12ether_header(%struct.ether_header* dereferenceable(12) %eth)
  call void @llvm.lifetime.end(i64 12, i8* %0) #1
  ret void
}

define void @tgt(%struct.ether_addr* nocapture readonly %ether_src, %struct.ether_addr* nocapture readonly %ether_dst) #0 {
entry:
  %eth = alloca %struct.ether_header, align 1
  %0 = getelementptr inbounds %struct.ether_header, %struct.ether_header* %eth, i64 0, i32 0, i64 0
  call void @llvm.lifetime.start(i64 12, i8* %0) #1
  %arraydecay = getelementptr inbounds %struct.ether_header, %struct.ether_header* %eth, i64 0, i32 1, i64 0
  %1 = getelementptr inbounds %struct.ether_addr, %struct.ether_addr* %ether_src, i64 0, i32 0, i64 0
  call void @llvm.memcpy.p0i8.p0i8.i64(i8* %arraydecay, i8* %1, i64 6, i32 1, i1 false), !tbaa.struct !1
  %2 = getelementptr inbounds %struct.ether_addr, %struct.ether_addr* %ether_dst, i64 0, i32 0, i64 0
  call void @llvm.memcpy.p0i8.p0i8.i64(i8* %0, i8* %2, i64 6, i32 1, i1 false), !tbaa.struct !1
  %3 = getelementptr i8, i8* %0, i64 6
  call void @llvm.memset.p0i8.i64(i8* %3, i8 0, i64 6, i32 1, i1 false)
  call void @_Z5PrintRK12ether_header(%struct.ether_header* dereferenceable(12) %eth)
  call void @llvm.lifetime.end(i64 12, i8* %0) #1
  ret void
}


; Function Attrs: nounwind
declare void @llvm.lifetime.start(i64, i8* nocapture) #1

; Function Attrs: nounwind
declare void @llvm.memset.p0i8.i64(i8* nocapture, i8, i64, i32, i1) #1

; Function Attrs: nounwind
declare void @llvm.memcpy.p0i8.p0i8.i64(i8* nocapture, i8* nocapture readonly, i64, i32, i1) #1

declare void @_Z5PrintRK12ether_header(%struct.ether_header* dereferenceable(12)) #2

; Function Attrs: nounwind
declare void @llvm.lifetime.end(i64, i8* nocapture) #1

attributes #0 = { uwtable "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+sse,+sse2" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { nounwind }
attributes #2 = { "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+sse,+sse2" "unsafe-fp-math"="false" "use-soft-float"="false" }

!llvm.ident = !{!0}

!0 = !{!"clang version 3.7.0 (trunk 237805) (llvm/trunk 237809)"}
!1 = !{i64 0, i64 6, !2}
!2 = !{!3, !3, i64 0}
!3 = !{!"omnipotent char", !4, i64 0}
!4 = !{!"Simple C/C++ TBAA"}

